// Departure-board layout: rows of route | destination | minutes, with an
// optional scrolling message line at the bottom.
#pragma once
#include <Adafruit_GFX.h>

struct Departure {
  char route[6];         // e.g. "250", "N133"
  char destination[40];  // e.g. "Croydon Town Centre"
  int minutes;           // 0 shows as "Due"
};

struct BoardOptions {
  uint16_t color;
  bool showMessage;     // true: 3 departure rows + message; false: 4 rows
  const char *message;  // scrolled along the bottom row when shown
};

// Draws one frame. Animation (message scroll, long-destination slide) is
// derived from nowMs, so the caller just redraws every ~30 ms.
void renderBoard(Adafruit_GFX &g, const Departure *deps, int count,
                 const BoardOptions &opt, uint32_t nowMs);

// Draws the scrolling message on the bottom row if enabled. Returns the
// number of rows left above it for other content.
int renderMessageRow(Adafruit_GFX &g, const BoardOptions &opt, uint32_t nowMs);

// Secret mode: rows of "bums" sliding in alternate directions.
void renderBums(Adafruit_GFX &g, uint16_t color, uint32_t nowMs);
