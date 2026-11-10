#include "feed.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

static const uint32_t POLL_MS = 30000;       // TfL and Darwin refresh ~30s
static const uint32_t STALE_MS = 5 * 60000;  // then show "No live data"
static const int DEPARTED_GRACE_S = 30;      // keep "Due" briefly past zero

struct Result {
  Departure deps[FEED_MAX];
  int secs[FEED_MAX];  // seconds until departure at fetch time
  int count;
  bool haveData;
  uint32_t fetchedAt;
  char error[48];
};

static SemaphoreHandle_t lock;
static TaskHandle_t task;
static FeedRequest req;  // written by the main loop, read by the task
static Result res;

static bool sameRequest(const FeedRequest &a, const FeedRequest &b) {
  return a.kind == b.kind && !strcmp(a.where, b.where) &&
         !strcmp(a.dir, b.dir) && !strcmp(a.secret, b.secret) &&
         !strcmp(a.base, b.base);
}

static bool isConfigured(const FeedRequest &r) {
  if (r.kind == FEED_BUS) return r.where[0] && r.secret[0];
  if (r.kind == FEED_TRAIN) return r.where[0] && r.secret[0] && r.base[0];
  return false;
}

// Inserts a departure in time order, keeping the soonest FEED_MAX.
static void insertSorted(Result &r, const char *route, const char *dest,
                         int secs, const char *status) {
  int pos = r.count;
  while (pos > 0 && r.secs[pos - 1] > secs) pos--;
  if (pos >= FEED_MAX) return;
  const int last = r.count < FEED_MAX ? r.count : FEED_MAX - 1;
  for (int i = last; i > pos; i--) {
    r.deps[i] = r.deps[i - 1];
    r.secs[i] = r.secs[i - 1];
  }
  Departure &d = r.deps[pos];
  strlcpy(d.route, route, sizeof(d.route));
  strlcpy(d.destination, dest, sizeof(d.destination));
  strlcpy(d.status, status ? status : "", sizeof(d.status));
  r.secs[pos] = secs;
  if (r.count < FEED_MAX) r.count++;
}

// GETs url and parses the JSON body through filter into doc. With
// buffered=true the whole body is read first, which copes with chunked
// replies (Railway's edge chunks even HTTP/1.0 responses); otherwise the
// body is streamed, which suits TfL's larger replies.
static bool getJson(const String &url, const char *headerName,
                    const char *headerValue, JsonDocument &filter,
                    JsonDocument &doc, Result &r, bool buffered = false) {
  // Verify servers against the framework's bundle of trusted root CAs.
  WiFiClientSecure client;
  client.useBuiltinCACertBundle();
  client.setHandshakeTimeout(15);  // seconds; the default is 120
  HTTPClient http;
  http.useHTTP10(!buffered);  // asks for no chunking, so the JSON can stream
  http.setTimeout(10000);
  if (!http.begin(client, url)) {
    strlcpy(r.error, "Bad URL", sizeof(r.error));
    return false;
  }
  if (headerName) http.addHeader(headerName, headerValue);
  const int code = http.GET();
  if (code != 200) {
    if (code < 0)
      snprintf(r.error, sizeof(r.error), "%s", http.errorToString(code).c_str());
    else
      snprintf(r.error, sizeof(r.error), "Server error %d", code);
    http.end();
    return false;
  }
  DeserializationError jerr;
  if (buffered) {
    const String body = http.getString();  // de-chunks the reply
    jerr = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  } else {
    jerr = deserializeJson(doc, http.getStream(),
                           DeserializationOption::Filter(filter));
  }
  http.end();
  if (jerr) {
    snprintf(r.error, sizeof(r.error), "Bad data: %s", jerr.c_str());
    return false;
  }
  return true;
}

// TfL: StopPoint/{naptanId}/Arrivals, an unsorted array of predictions.
static bool fetchBus(const FeedRequest &q, Result &r) {
  JsonDocument filter, doc;
  filter[0]["lineName"] = true;
  filter[0]["destinationName"] = true;
  filter[0]["timeToStation"] = true;
  String url = String("https://api.tfl.gov.uk/StopPoint/") + q.where +
               "/Arrivals?app_key=" + q.secret;
  if (!getJson(url, nullptr, nullptr, filter, doc, r)) return false;
  for (JsonObject a : doc.as<JsonArray>()) {
    // "Clapham Park, Atkins Road" -> "Clapham Park"
    char dest[sizeof(Departure::destination)];
    strlcpy(dest, a["destinationName"] | "", sizeof(dest));
    if (char *comma = strchr(dest, ',')) *comma = '\0';
    insertSorted(r, a["lineName"] | "?", dest, a["timeToStation"] | 99999,
                 nullptr);
  }
  return true;
}

// Proxy: /trains/{crs}?dir=..., already filtered and in time order.
static bool fetchTrain(const FeedRequest &q, Result &r) {
  JsonDocument filter, doc;
  filter["services"][0]["std"] = true;
  filter["services"][0]["dest"] = true;
  filter["services"][0]["mins"] = true;
  filter["services"][0]["status"] = true;
  String base = q.base;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  String url = base + "/trains/" + q.where + "?dir=" + q.dir;
  if (!getJson(url, "X-Board-Token", q.secret, filter, doc, r, true))
    return false;
  if (!doc["services"].is<JsonArray>()) {
    strlcpy(r.error, "Unexpected reply", sizeof(r.error));
    return false;
  }
  for (JsonObject s : doc["services"].as<JsonArray>()) {
    const int mins = s["mins"] | 0;
    insertSorted(r, s["std"] | "", s["dest"] | "", mins * 60,
                 s["status"] | (const char *)nullptr);
  }
  return true;
}

static bool fetch(const FeedRequest &q, Result &r) {
  r.count = 0;
  r.error[0] = '\0';
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(r.error, "No Wi-Fi", sizeof(r.error));
    return false;
  }
  return q.kind == FEED_BUS ? fetchBus(q, r) : fetchTrain(q, r);
}

static void taskFn(void *) {
  static Result fresh;  // static: too big for comfort on the task stack
  static FeedRequest q;
  esp_task_wdt_add(nullptr);  // restart the board if this task hangs
  // Let Matter's startup memory spike pass before the first TLS handshake.
  for (int i = 0; i < 3; i++) {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  for (;;) {
    esp_task_wdt_reset();
    xSemaphoreTake(lock, portMAX_DELAY);
    q = req;
    xSemaphoreGive(lock);

    if (isConfigured(q)) {
      const bool ok = fetch(q, fresh);
      Serial.printf("Feed %s: %s (%d)\n", q.where, ok ? "ok" : fresh.error,
                    fresh.count);
      xSemaphoreTake(lock, portMAX_DELAY);
      if (sameRequest(q, req)) {  // ignore if the mode changed meanwhile
        if (ok) {
          memcpy(res.deps, fresh.deps, sizeof(res.deps));
          memcpy(res.secs, fresh.secs, sizeof(res.secs));
          res.count = fresh.count;
          res.haveData = true;
          res.fetchedAt = millis();
          res.error[0] = '\0';
        } else {
          strlcpy(res.error, fresh.error, sizeof(res.error));
        }
      }
      xSemaphoreGive(lock);
    }
    // Sleep until the next poll, or wake early when the request changes.
    // Waits in slices so the watchdog keeps being fed.
    for (uint32_t waited = 0; waited < POLL_MS; waited += 5000) {
      esp_task_wdt_reset();
      if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000))) break;
    }
  }
}

void feedStatusText(char *buf, size_t len) {
  xSemaphoreTake(lock, portMAX_DELAY);
  if (!isConfigured(req)) {
    snprintf(buf, len, "%s", req.kind == FEED_NONE ? "Idle" : "Not configured");
  } else if (res.error[0]) {
    snprintf(buf, len, "%s: %s", req.where, res.error);
  } else if (res.haveData) {
    snprintf(buf, len, "%s: ok, %lu s ago", req.where,
             (unsigned long)((millis() - res.fetchedAt) / 1000));
  } else {
    snprintf(buf, len, "%s: loading", req.where);
  }
  xSemaphoreGive(lock);
}

void feedBegin() {
  lock = xSemaphoreCreateMutex();
  // Stack in PSRAM: internal RAM is reserved for TLS, Matter and display DMA.
  if (xTaskCreatePinnedToCoreWithCaps(taskFn, "feed", 12288, nullptr, 1, &task,
                                      0, MALLOC_CAP_SPIRAM) != pdPASS) {
    Serial.println("Feed task in PSRAM failed; using internal RAM");
    xTaskCreatePinnedToCore(taskFn, "feed", 12288, nullptr, 1, &task, 0);
  }
}

void feedSet(const FeedRequest &r) {
  xSemaphoreTake(lock, portMAX_DELAY);
  const bool changed = !sameRequest(r, req);
  if (changed) {
    req = r;
    res.count = 0;
    res.haveData = false;
    res.error[0] = '\0';
  }
  xSemaphoreGive(lock);
  if (changed) xTaskNotifyGive(task);
}

FeedStatus feedSnapshot(Departure *out, int &count, char *err, size_t errLen) {
  count = 0;
  err[0] = '\0';
  xSemaphoreTake(lock, portMAX_DELAY);
  FeedStatus status;
  if (!isConfigured(req)) {
    status = FEED_NOT_CONFIGURED;
  } else if (!res.haveData) {
    status = res.error[0] ? FEED_ERROR : FEED_LOADING;
    strlcpy(err, res.error, errLen);
  } else if (millis() - res.fetchedAt > STALE_MS) {
    status = FEED_ERROR;
    strlcpy(err, res.error[0] ? res.error : "Data out of date", errLen);
  } else {
    status = FEED_OK;
    const int elapsed = (millis() - res.fetchedAt) / 1000;
    for (int i = 0; i < res.count; i++) {
      const int secs = res.secs[i] - elapsed;
      if (secs < -DEPARTED_GRACE_S) continue;  // already gone
      out[count] = res.deps[i];
      out[count].minutes = secs < 60 ? 0 : secs / 60;
      count++;
    }
  }
  xSemaphoreGive(lock);
  return status;
}
