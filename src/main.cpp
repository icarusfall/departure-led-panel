// Home Departure Board
// Live TfL bus departures, settings page, modes and night schedule.

#include <Arduino.h>
#include "board.h"
#include "bus_feed.h"
#include "display.h"
#include "dotfont.h"
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

static void renderTrainPlaceholder(const BoardOptions &opt, uint32_t now) {
  const ModeInfo &m = MODES[settings.mode];
  matrix.fillScreen(0);
  renderMessageRow(matrix, opt, now);
  dotfont::drawText(matrix, 0, 0, m.place, opt.color);
  dotfont::drawText(matrix, 0, 8, m.dir, opt.color);
  dotfont::drawText(matrix, 0, 16, "Train times coming soon", opt.color);
}

// Up to two lines of status text above the message row.
static void renderNotice(const BoardOptions &opt, uint32_t now,
                         const char *a, const char *b = nullptr) {
  matrix.fillScreen(0);
  renderMessageRow(matrix, opt, now);
  dotfont::drawText(matrix, 0, 0, a, opt.color, 0, matrix.width());
  if (b && *b) dotfont::drawText(matrix, 0, 8, b, opt.color, 0, matrix.width());
}

static void renderBuses(const BoardOptions &opt, uint32_t now) {
  static Departure deps[BUS_MAX];
  int count;
  char err[48];
  switch (busFeedSnapshot(deps, count, err, sizeof(err))) {
    case BUS_NOT_CONFIGURED:
      renderNotice(opt, now, "Set bus stop and TfL key",
                   "on the settings page");
      break;
    case BUS_LOADING:
      renderNotice(opt, now, MODES[settings.mode].title, "Loading...");
      break;
    case BUS_ERROR:
      renderNotice(opt, now, "No live data", err);
      break;
    case BUS_OK:
      renderBoard(matrix, deps, count, opt, now);
      break;
  }
}

// Points the bus feed at the active mode's stop (or pauses it).
static void updateBusFeed() {
  if (settings.mode == MODE_BUS_NORTH)
    busFeedSetStop(settings.busStopNorth, settings.tflKey);
  else if (settings.mode == MODE_BUS_SOUTH)
    busFeedSetStop(settings.busStopSouth, settings.tflKey);
  else
    busFeedSetStop("", "");
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
  busFeedBegin();
  onModeChange();  // show the current mode's name on boot
}

void loop() {
  webLoop();

  static uint32_t lastFrame = 0;
  const uint32_t now = millis();
  updateBusFeed();
  if (now - lastFrame < FRAME_MS) return;
  lastFrame = now;

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

  if (settings.mode == MODE_BUMS) {
    renderBums(matrix, opt.color, now);
  } else if (modeIsTrain(settings.mode)) {
    renderTrainPlaceholder(opt, now);
  } else {
    renderBuses(opt, now);
  }
  matrix.show();
}
