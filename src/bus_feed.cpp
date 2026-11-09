#include "bus_feed.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static const uint32_t POLL_MS = 30000;        // TfL predictions refresh ~30s
static const uint32_t STALE_MS = 5 * 60000;   // then show "No live data"
static const int DEPARTED_GRACE_S = 30;       // keep "Due" briefly past zero

struct Result {
  Departure deps[BUS_MAX];
  int secs[BUS_MAX];  // TfL timeToStation at fetch time
  int count;
  bool haveData;
  uint32_t fetchedAt;
  char error[48];
};

static SemaphoreHandle_t lock;
static TaskHandle_t task;
static char reqStop[16];  // written by the main loop, read by the task
static char reqKey[64];
static Result res;

// "Clapham Park, Atkins Road" -> "Clapham Park"
static void shortenDestination(const char *in, char *out, size_t len) {
  strlcpy(out, in, len);
  char *comma = strchr(out, ',');
  if (comma) *comma = '\0';
}

static bool fetch(const char *stop, const char *key, Result &r) {
  r.count = 0;
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(r.error, "No Wi-Fi", sizeof(r.error));
    return false;
  }

  // TODO(milestone 6): verify TfL's certificate instead of skipping checks.
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding, so the JSON can stream
  http.setTimeout(10000);
  String url = String("https://api.tfl.gov.uk/StopPoint/") + stop +
               "/Arrivals?app_key=" + key;
  if (!http.begin(client, url)) {
    strlcpy(r.error, "Bad URL", sizeof(r.error));
    return false;
  }
  const int code = http.GET();
  if (code != 200) {
    if (code < 0)
      snprintf(r.error, sizeof(r.error), "%s", http.errorToString(code).c_str());
    else
      snprintf(r.error, sizeof(r.error), "TfL error %d", code);
    http.end();
    return false;
  }

  // Keep only the fields we use; the full response is much larger.
  JsonDocument filter;
  filter[0]["lineName"] = true;
  filter[0]["destinationName"] = true;
  filter[0]["timeToStation"] = true;
  JsonDocument doc;
  DeserializationError jerr = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (jerr) {
    snprintf(r.error, sizeof(r.error), "Bad data: %s", jerr.c_str());
    return false;
  }

  // Insert each arrival in time order, keeping the soonest BUS_MAX.
  for (JsonObject a : doc.as<JsonArray>()) {
    const int secs = a["timeToStation"] | 99999;
    int pos = r.count;
    while (pos > 0 && r.secs[pos - 1] > secs) pos--;
    if (pos >= BUS_MAX) continue;
    const int last = r.count < BUS_MAX ? r.count : BUS_MAX - 1;
    for (int i = last; i > pos; i--) {
      r.deps[i] = r.deps[i - 1];
      r.secs[i] = r.secs[i - 1];
    }
    strlcpy(r.deps[pos].route, a["lineName"] | "?", sizeof(r.deps[pos].route));
    shortenDestination(a["destinationName"] | "", r.deps[pos].destination,
                       sizeof(r.deps[pos].destination));
    r.secs[pos] = secs;
    if (r.count < BUS_MAX) r.count++;
  }
  r.error[0] = '\0';
  return true;
}

static void taskFn(void *) {
  static Result fresh;  // static: too big for comfort on the task stack
  for (;;) {
    char stop[sizeof(reqStop)], key[sizeof(reqKey)];
    xSemaphoreTake(lock, portMAX_DELAY);
    strlcpy(stop, reqStop, sizeof(stop));
    strlcpy(key, reqKey, sizeof(key));
    xSemaphoreGive(lock);

    if (stop[0] && key[0]) {
      const bool ok = fetch(stop, key, fresh);
      Serial.printf("TfL %s: %s (%d arrivals)\n", stop,
                    ok ? "ok" : fresh.error, fresh.count);
      xSemaphoreTake(lock, portMAX_DELAY);
      if (strcmp(stop, reqStop) == 0) {  // ignore if the mode changed meanwhile
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
    // Sleep until the next poll, or wake early when the stop changes.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(POLL_MS));
  }
}

void busFeedBegin() {
  lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(taskFn, "busFeed", 12288, nullptr, 1, &task, 0);
}

void busFeedSetStop(const char *naptanId, const char *appKey) {
  xSemaphoreTake(lock, portMAX_DELAY);
  const bool changed =
      strcmp(naptanId, reqStop) != 0 || strcmp(appKey, reqKey) != 0;
  if (changed) {
    strlcpy(reqStop, naptanId, sizeof(reqStop));
    strlcpy(reqKey, appKey, sizeof(reqKey));
    res.count = 0;
    res.haveData = false;
    res.error[0] = '\0';
  }
  xSemaphoreGive(lock);
  if (changed) xTaskNotifyGive(task);
}

BusStatus busFeedSnapshot(Departure *out, int &count, char *err,
                          size_t errLen) {
  count = 0;
  err[0] = '\0';
  xSemaphoreTake(lock, portMAX_DELAY);
  BusStatus status;
  if (!reqStop[0] || !reqKey[0]) {
    status = BUS_NOT_CONFIGURED;
  } else if (!res.haveData) {
    status = res.error[0] ? BUS_ERROR : BUS_LOADING;
    strlcpy(err, res.error, errLen);
  } else if (millis() - res.fetchedAt > STALE_MS) {
    status = BUS_ERROR;
    strlcpy(err, res.error[0] ? res.error : "Data out of date", errLen);
  } else {
    status = BUS_OK;
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
