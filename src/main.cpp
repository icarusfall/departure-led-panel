// Home Departure Board
// Live buses (TfL) and trains (Darwin via our Railway proxy), settings
// page, modes and night schedule.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <string.h>
#include "board.h"
#include "display.h"
#include "dotfont.h"
#include "feed.h"
#include "matter_modes.h"
#include "modes.h"
#include "net.h"
#include "ota.h"
#include "settings.h"
#include "status.h"
#include "web.h"

static const uint32_t FRAME_MS = 25;
static const uint32_t TITLE_MS = 2500;      // mode name shown after a change
static const uint32_t WATCHDOG_MS = 60000;  // restart if a task hangs this long
static const uint32_t HEARTBEAT_MS = 60000; // status line in the serial log

// Restarts the board if the main loop (or the feed task, which adds itself)
// stops checking in for WATCHDOG_MS.
static void watchdogBegin() {
  esp_task_wdt_config_t cfg = {};
  cfg.timeout_ms = WATCHDOG_MS;
  cfg.idle_core_mask = 0;
  cfg.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
  esp_task_wdt_add(nullptr);
}

static void heartbeat(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < HEARTBEAT_MS) return;
  last = now;
  char feed[64];
  feedStatusText(feed, sizeof(feed));
  Serial.printf("[%lus] mode=%u wifi=%s rssi=%d heap=%uK min=%uK matter=%s | %s\n",
                (unsigned long)(now / 1000), settings.mode,
                netConnected() ? "up" : "DOWN", WiFi.RSSI(),
                (unsigned)(ESP.getFreeHeap() / 1024),
                (unsigned)(ESP.getMinFreeHeap() / 1024),
                matterModesCommissioned() ? "paired" : "unpaired", feed);
}

static uint32_t titleUntil = 0;

static void logHeap(const char *stage) {
  Serial.printf("[heap] %-14s free=%uK min=%uK largest=%uK psram=%uK\n", stage,
                (unsigned)(ESP.getFreeHeap() / 1024),
                (unsigned)(ESP.getMinFreeHeap() / 1024),
                (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
                (unsigned)(ESP.getFreePsram() / 1024));
  Serial.flush();
}

// Starts Matter unless the previous attempt crashed the board. A flag is
// set in NVS before starting and cleared once it's up, so a crash during
// startup leaves it set and the next boot runs without voice control
// rather than crash-looping.
static bool matterSafeBegin() {
  Preferences p;
  p.begin("boot", false);
  const bool crashedLastTime = p.getBool("matterTry", false);
  if (crashedLastTime) {
    p.putBool("matterTry", false);  // try again on the boot after this one
    p.end();
    Serial.println("Matter skipped: it crashed on the previous boot");
    return false;
  }
  p.putBool("matterTry", true);
  p.end();
  matterModesBegin(settings.mode);
  p.begin("boot", false);
  p.putBool("matterTry", false);
  p.end();
  return true;
}

// Called when the mode changes on the settings page, and at boot.
static void onModeChange() {
  titleUntil = millis() + TITLE_MS;
  matterModesShow(settings.mode);  // keep Google Home in step
}

// Applies a mode chosen from Google Home.
static void applyMatterRequest() {
  const int m = matterModesTakeRequest();
  if (m < 0 || m >= MODE_COUNT || m == settings.mode) return;
  settings.mode = m;
  settingsSave();
  titleUntil = millis() + TITLE_MS;
}

// True when the night schedule says the board should be dimmed or off.
static bool isNight() {
  if (!settings.nightEnabled) return false;
  int now;
  if (!netLocalMinutes(now)) return false;
  const int a = settings.nightStart, b = settings.nightEnd;
  if (a == b) return false;
  return a < b ? (now >= a && now < b) : (now >= a || now < b);
}

// Up to two lines of status text above the message row.
static void renderNotice(const BoardOptions &opt, uint32_t now,
                         const char *a, const char *b = nullptr) {
  matrix.fillScreen(0);
  renderMessageRow(matrix, opt, now);
  dotfont::drawText(matrix, 0, 0, a, opt.color, 0, matrix.width());
  if (b && *b) dotfont::drawText(matrix, 0, 8, b, opt.color, 0, matrix.width());
}

// What the feed should poll for the active mode.
static FeedRequest requestForMode(uint8_t mode) {
  FeedRequest r;
  const char *stop = nullptr, *crs = nullptr, *dir = nullptr;
  switch (mode) {
    case MODE_BUS_NORTH: stop = settings.busStopNorth; break;
    case MODE_BUS_SOUTH: stop = settings.busStopSouth; break;
    case MODE_STREATHAM_NORTH: crs = "STE"; dir = "north"; break;
    case MODE_STC_NORTH: crs = "SRC"; dir = "north"; break;
    case MODE_STC_SOUTH: crs = "SRC"; dir = "south"; break;
    default: break;  // bums: nothing to fetch
  }
  if (stop) {
    r.kind = FEED_BUS;
    strlcpy(r.where, stop, sizeof(r.where));
    strlcpy(r.secret, settings.tflKey, sizeof(r.secret));
  } else if (crs) {
    r.kind = FEED_TRAIN;
    strlcpy(r.where, crs, sizeof(r.where));
    strlcpy(r.dir, dir, sizeof(r.dir));
    strlcpy(r.secret, settings.proxyToken, sizeof(r.secret));
    strlcpy(r.base, settings.proxyUrl, sizeof(r.base));
  }
  return r;
}

static void renderDepartures(BoardOptions &opt, uint32_t now) {
  static Departure deps[FEED_MAX];
  const bool train = modeIsTrain(settings.mode);
  int count;
  char err[48];
  switch (feedSnapshot(deps, count, err, sizeof(err))) {
    case FEED_NOT_CONFIGURED:
      renderNotice(opt, now,
                   train ? "Set train proxy token" : "Set bus stop and TfL key",
                   "on the settings page");
      break;
    case FEED_LOADING:
      renderNotice(opt, now, MODES[settings.mode].title, "Loading...");
      break;
    case FEED_ERROR:
      renderNotice(opt, now, "No live data", err);
      break;
    case FEED_OK:
      opt.routeWidth = train ? ROUTE_COL_TRAIN : ROUTE_COL_BUS;
      renderBoard(matrix, deps, count, opt, now);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("Departure board: %d panel(s), %dx%d, brightness cap %d\n",
                PANEL_COUNT, MATRIX_W, MATRIX_H, BOARD_MAX_BRIGHTNESS);
  Serial.printf("Last restart: %s. Firmware " __DATE__ " " __TIME__ "\n",
                resetReasonText());

  settingsLoad();
  displaySetBrightness(settings.brightness);
  // Send ordinary allocations over 64 bytes to PSRAM (2MB, mostly unused)
  // instead of the default 4KB threshold. That leaves internal RAM for what
  // must live there: display DMA buffers, TLS (forced internal in this
  // build) and Matter's startup.
  heap_caps_malloc_extmem_enable(64);
  logHeap("boot");
  if (!displayBegin()) {
    for (;;) delay(1000);
  }
  displayTestPattern(dimColor(settings.color));

  logHeap("after display");
  netBegin();  // may block in the setup portal, so before the watchdog
  webBegin(onModeChange);
  otaBegin(settings.otaPassword);
  // Matter starts before the first data fetch so their memory peaks
  // (Matter startup, TLS handshakes) don't overlap.
  logHeap("before Matter");
  matterSafeBegin();
  logHeap("after Matter");
  watchdogBegin();
  feedBegin();
  onModeChange();  // show the current mode's name on boot
}

void loop() {
  esp_task_wdt_reset();
  netLoop();
  otaLoop();
  webLoop();
  matterModesLoop();
  applyMatterRequest();
  heartbeat(millis());

  static uint32_t lastFrame = 0;
  const uint32_t now = millis();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;

  feedSet(requestForMode(settings.mode));

  const bool night = isNight();
  if (night && settings.nightOff) {
    matrix.fillScreen(0);
    matrix.show();
    return;
  }
  displaySetBrightness(night ? settings.nightBrightness : settings.brightness);

  BoardOptions opt;
  opt.color = dimColor(settings.color);
  opt.showMessage = settings.showMessage;
  opt.message = settings.message;

  if ((int32_t)(titleUntil - now) > 0) {
    displayLines(MODES[settings.mode].title, nullptr, nullptr, opt.color);
    return;  // displayLines already called show()
  }

  if (settings.mode == MODE_BUMS)
    renderBums(matrix, opt.color, now);
  else
    renderDepartures(opt, now);
  matrix.show();
}
