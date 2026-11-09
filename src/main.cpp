// Home Departure Board
// Milestone 2: departure-board layout with made-up data.

#include <Arduino.h>
#include "board.h"
#include "display.h"
#include "fake_feed.h"

static const uint32_t FRAME_MS = 25;

// Runtime options. These move to the on-device settings page in milestone 3.
static BoardOptions options;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.printf("Departure board: %d panel(s), %dx%d, brightness cap %d\n",
                PANEL_COUNT, MATRIX_W, MATRIX_H, MAX_BRIGHTNESS);

  if (!displayBegin()) {
    for (;;) delay(1000);
  }

  options.color = dimColor(255, 140, 0);  // amber
  options.showMessage = true;
  options.message = "Rufus Rules!  Live departures coming soon";

  displayTestPattern(options.color);
  fakeFeedBegin();
}

void loop() {
  static uint32_t lastFrame = 0;
  const uint32_t now = millis();
  fakeFeedUpdate(now);
  if (now - lastFrame >= FRAME_MS) {
    lastFrame = now;
    renderBoard(matrix, fakeFeedList(), fakeFeedCount(), options, now);
    matrix.show();
  }
}
