// HUB75 matrix setup and colour helpers.
#pragma once
#include <Adafruit_Protomatter.h>
#include "config.h"

extern Adafruit_Protomatter matrix;

// Starts the matrix. Returns false (and logs) on failure.
bool displayBegin();

// Sets the brightness (5-100, percent of the MAX_BRIGHTNESS hardware cap)
// used by the colour helpers below.
void displaySetBrightness(uint8_t pct);

// Scales a colour by the current brightness and packs it to RGB565.
uint16_t dimColor(uint8_t r, uint8_t g, uint8_t b);
uint16_t dimColor(uint32_t rgb);

// Clears the screen and draws up to three centred lines of text.
void displayLines(const char *a, const char *b = nullptr,
                  const char *c = nullptr, uint16_t color = 0);

// Short boot test: R/G/B/white fills, then numbered panel boxes.
void displayTestPattern(uint16_t accent);
