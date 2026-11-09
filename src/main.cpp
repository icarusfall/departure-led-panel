// Milestone 1: Hello panel.
// Shows a short R/G/B/white test pattern, then a static amber departure line
// with a scrolling hello message underneath.

#include <Arduino.h>
#include <Adafruit_Protomatter.h>

#ifndef PANEL_COUNT
#define PANEL_COUNT 1
#endif
#ifndef MAX_BRIGHTNESS
#define MAX_BRIGHTNESS 60
#endif

constexpr int PANEL_W = 64;
constexpr int PANEL_H = 32;
constexpr int WIDTH = PANEL_W * PANEL_COUNT;

// HUB75 pins taken from Adafruit_Protomatter/examples/simple/simple.ino
// (ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3 block): {42, 41, 40, 38, 39, 37}.
// Waveshare P2.5 panels have green and blue swapped, so G/B pins are
// exchanged here (order is R1, G1, B1, R2, G2, B2).
uint8_t rgbPins[] = {42, 40, 41, 38, 37, 39};
uint8_t addrPins[] = {45, 36, 48, 35, 21};
uint8_t clockPin = 2;
uint8_t latchPin = 47;
uint8_t oePin = 14;

// 32-row, 1/16 scan panels use 4 address lines (A-D). Chained panels are
// treated as one wide matrix. Bit depth 4, double-buffered for smooth scroll.
Adafruit_Protomatter matrix(WIDTH, 4, 1, rgbPins, 4, addrPins,
                            clockPin, latchPin, oePin, true);

// Scale an 8-bit RGB colour by the brightness cap and pack to RGB565.
uint16_t dim(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.color565((r * MAX_BRIGHTNESS) / 255, (g * MAX_BRIGHTNESS) / 255,
                         (b * MAX_BRIGHTNESS) / 255);
}

uint16_t AMBER;

void testPattern() {
  struct { const char *name; uint16_t c; } steps[] = {
      {"RED", dim(255, 0, 0)},
      {"GREEN", dim(0, 255, 0)},
      {"BLUE", dim(0, 0, 255)},
      {"WHITE", dim(255, 255, 255)},
  };
  for (auto &s : steps) {
    matrix.fillScreen(0);
    matrix.fillRect(0, 0, WIDTH, PANEL_H, s.c);
    matrix.setTextColor(0);
    matrix.setCursor(2, 12);
    matrix.print(s.name);
    matrix.show();
    Serial.printf("Test pattern: %s\n", s.name);
    delay(1500);
  }
  // Per-panel index so you can check chain order: "1", "2", "3" left to right.
  matrix.fillScreen(0);
  matrix.setTextColor(AMBER);
  for (int p = 0; p < PANEL_COUNT; p++) {
    matrix.drawRect(p * PANEL_W, 0, PANEL_W, PANEL_H, AMBER);
    matrix.setCursor(p * PANEL_W + 29, 12);
    matrix.print(p + 1);
  }
  matrix.show();
  delay(2000);
}

const char *scrollMsg = "Hello world from Streatham!";
int scrollX = WIDTH;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("Hello panel: %d panel(s), %dx%d, brightness cap %d\n",
                PANEL_COUNT, WIDTH, PANEL_H, MAX_BRIGHTNESS);

  ProtomatterStatus status = matrix.begin();
  Serial.printf("Protomatter begin() status: %d\n", (int)status);
  if (status != PROTOMATTER_OK) {
    for (;;) delay(1000);
  }

  AMBER = dim(255, 140, 0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  testPattern();
}

void loop() {
  matrix.fillScreen(0);
  matrix.setTextColor(AMBER);

  // Default GFX font is 6px per character including spacing.
  if (WIDTH >= 128) {
    matrix.setCursor(0, 4);
    matrix.print("Rufus Rules!");
  } else {
    // A single 64px panel fits 10 characters, so split over two lines.
    matrix.setCursor(0, 0);
    matrix.print("Rufus");
    matrix.setCursor(0, 9);
    matrix.print("Rules!");
  }

  // Scrolling line on the bottom row.
  matrix.setCursor(scrollX, 24);
  matrix.print(scrollMsg);
  int msgW = strlen(scrollMsg) * 6;
  if (--scrollX < -msgW) scrollX = WIDTH;

  matrix.show();
  delay(30);
}
