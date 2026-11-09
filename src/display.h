// HUB75 matrix setup and colour helpers.
#pragma once
#include <Adafruit_Protomatter.h>
#include "config.h"

extern Adafruit_Protomatter matrix;

// Starts the matrix. Returns false (and logs) on failure.
bool displayBegin();

// Scales an 8-bit RGB colour by the brightness cap and packs it to RGB565.
uint16_t dimColor(uint8_t r, uint8_t g, uint8_t b);

// Short boot test: R/G/B/white fills, then numbered panel boxes.
void displayTestPattern(uint16_t accent);
