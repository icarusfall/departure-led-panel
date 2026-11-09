#include "display.h"
#include "dotfont.h"

// HUB75 pins taken from Adafruit_Protomatter/examples/simple/simple.ino
// (ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3 block): {42, 41, 40, 38, 39, 37}.
// Waveshare P2.5 panels have green and blue swapped, so G/B pins are
// exchanged here (order is R1, G1, B1, R2, G2, B2).
static uint8_t rgbPins[] = {42, 40, 41, 38, 37, 39};
static uint8_t addrPins[] = {45, 36, 48, 35, 21};
static const uint8_t clockPin = 2;
static const uint8_t latchPin = 47;
static const uint8_t oePin = 14;

// 32-row, 1/16 scan panels use 4 address lines (A-D). Chained panels are
// treated as one wide matrix. Bit depth 4, double-buffered for smooth scroll.
Adafruit_Protomatter matrix(MATRIX_W, 4, 1, rgbPins, 4, addrPins, clockPin,
                            latchPin, oePin, true);

bool displayBegin() {
  ProtomatterStatus status = matrix.begin();
  Serial.printf("Protomatter begin() status: %d\n", (int)status);
  if (status != PROTOMATTER_OK) return false;
  matrix.setRotation(DISPLAY_ROTATION);
  matrix.setTextWrap(false);
  return true;
}

uint16_t dimColor(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.color565((r * MAX_BRIGHTNESS) / 255,
                         (g * MAX_BRIGHTNESS) / 255,
                         (b * MAX_BRIGHTNESS) / 255);
}

void displayTestPattern(uint16_t accent) {
  struct { const char *name; uint16_t c; } steps[] = {
      {"RED", dimColor(255, 0, 0)},
      {"GREEN", dimColor(0, 255, 0)},
      {"BLUE", dimColor(0, 0, 255)},
      {"WHITE", dimColor(255, 255, 255)},
  };
  for (auto &s : steps) {
    matrix.fillScreen(s.c);
    dotfont::drawText(matrix, 2, 12, s.name, 0);
    matrix.show();
    delay(600);
  }
  // Numbered box per panel, to check chain order left to right.
  matrix.fillScreen(0);
  for (int p = 0; p < PANEL_COUNT; p++) {
    char label[4];
    snprintf(label, sizeof(label), "%d", p + 1);
    matrix.drawRect(p * PANEL_W, 0, PANEL_W, PANEL_H, accent);
    dotfont::drawText(matrix, p * PANEL_W + 30, 12, label, accent);
  }
  matrix.show();
  delay(1200);
}
