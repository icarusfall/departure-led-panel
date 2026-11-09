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
// treated as one wide matrix. Bit depth 6 gives smooth low-brightness
// steps; double-buffered for smooth scrolling.
Adafruit_Protomatter matrix(MATRIX_W, 6, 1, rgbPins, 4, addrPins, clockPin,
                            latchPin, oePin, true);

static uint8_t brightnessPct = 50;

bool displayBegin() {
  ProtomatterStatus status = matrix.begin();
  Serial.printf("Protomatter begin() status: %d\n", (int)status);
  if (status != PROTOMATTER_OK) return false;
  matrix.setRotation(DISPLAY_ROTATION);
  matrix.setTextWrap(false);
  return true;
}

void displaySetBrightness(uint8_t pct) {
  brightnessPct = pct < 1 ? 1 : (pct > 100 ? 100 : pct);
}

uint16_t dimColor(uint8_t r, uint8_t g, uint8_t b) {
  const uint32_t level = (uint32_t)MAX_BRIGHTNESS * brightnessPct / 100;
  return matrix.color565(r * level / 255, g * level / 255, b * level / 255);
}

uint16_t dimColor(uint32_t rgb) {
  return dimColor((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

void displayLines(const char *a, const char *b, const char *c,
                  uint16_t color) {
  if (!color) color = dimColor(0xFF8C00);
  const char *lines[] = {a, b, c};
  int n = 0;
  for (auto l : lines) if (l && *l) n++;
  matrix.fillScreen(0);
  int y = (MATRIX_H - n * 10) / 2 + 1;
  for (auto l : lines) {
    if (!l || !*l) continue;
    dotfont::drawText(matrix, (matrix.width() - dotfont::textWidth(l)) / 2,
                      y, l, color, 0, matrix.width());
    y += 10;
  }
  matrix.show();
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
    delay(400);
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
  delay(1000);
}
