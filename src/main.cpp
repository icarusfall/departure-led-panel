// Home Departure Board
// Milestone 3: Wi-Fi provisioning, settings page, modes and night schedule.
// Bus modes still use made-up departures until milestone 4.

#include <Arduino.h>
#include "board.h"
#include "display.h"
#include "dotfont.h"
#include "fake_feed.h"
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
  fakeFeedBegin();
  onModeChange();  // show the current mode's name on boot
}

void loop() {
  webLoop();

  static uint32_t lastFrame = 0;
  const uint32_t now = millis();
  fakeFeedUpdate(now);
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
    renderBoard(matrix, fakeFeedList(), fakeFeedCount(), opt, now);
  }
  matrix.show();
}
