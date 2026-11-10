// Home Departure Board
// Live buses (TfL) and trains (Darwin via our Railway proxy), settings
// page, modes and night schedule.

#include <Arduino.h>
#include <string.h>
#include "board.h"
#include "display.h"
#include "dotfont.h"
#include "feed.h"
#include "modes.h"
#include "net.h"
#include "settings.h"
#include "web.h"

static const uint32_t FRAME_MS = 25;
static const uint32_t TITLE_MS = 2500;  // mode name shown after a change

static uint32_t titleUntil = 0;

static void onModeChange() { titleUntil = millis() + TITLE_MS; }

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
                PANEL_COUNT, MATRIX_W, MATRIX_H, MAX_BRIGHTNESS);

  settingsLoad();
  displaySetBrightness(settings.brightness);
  if (!displayBegin()) {
    for (;;) delay(1000);
  }
  displayTestPattern(dimColor(settings.color));

  netBegin();
  webBegin(onModeChange);
  feedBegin();
  onModeChange();  // show the current mode's name on boot
}

void loop() {
  webLoop();

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
