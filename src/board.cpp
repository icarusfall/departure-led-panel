#include "board.h"
#include <stdio.h>
#include "dotfont.h"

static const int ROW_H = 8;          // 7px glyphs + descender row; 4 rows = 32px
static const int ROUTE_W = 23;       // fits four tabular characters, e.g. "N133"
static const int COL_GAP = 3;
static const int MSG_MS_PER_PX = 30;  // message scroll speed (~33 px/s)

// Long destinations: hold, slide left to show the end, hold, jump back.
static const int SLIDE_HOLD_START_MS = 2500;
static const int SLIDE_HOLD_END_MS = 1500;
static const int SLIDE_MS_PER_PX = 45;

static void formatMinutes(int minutes, char *buf, size_t len) {
  if (minutes <= 0)
    snprintf(buf, len, "Due");
  else
    snprintf(buf, len, "%d min", minutes);
}

static void drawRow(Adafruit_GFX &g, int row, const Departure &d,
                    uint16_t color, uint32_t nowMs) {
  const int y = row * ROW_H;
  const int width = g.width();

  dotfont::drawText(g, 0, y, d.route, color, 0, ROUTE_W);

  // Minutes, right-aligned. The column is sized for the widest likely value
  // so the destination column doesn't jump around as times change.
  static int minColW = -1;
  if (minColW < 0) minColW = dotfont::textWidth("88 min");
  char mins[12];
  formatMinutes(d.minutes, mins, sizeof(mins));
  dotfont::drawText(g, width - dotfont::textWidth(mins), y, mins, color);

  // Destination, clipped to its column; slides if it doesn't fit.
  const int destL = ROUTE_W + COL_GAP;
  const int destR = width - minColW - COL_GAP;
  const int overflow = dotfont::textWidth(d.destination) - (destR - destL);
  int offset = 0;
  if (overflow > 0) {
    const uint32_t cycle =
        SLIDE_HOLD_START_MS + overflow * SLIDE_MS_PER_PX + SLIDE_HOLD_END_MS;
    // Stagger rows so several long names don't move in lockstep.
    const uint32_t t = (nowMs + row * 900) % cycle;
    if (t > (uint32_t)SLIDE_HOLD_START_MS) {
      offset = (t - SLIDE_HOLD_START_MS) / SLIDE_MS_PER_PX;
      if (offset > overflow) offset = overflow;
    }
  }
  dotfont::drawText(g, destL - offset, y, d.destination, color, destL, destR);
}

void renderBoard(Adafruit_GFX &g, const Departure *deps, int count,
                 const BoardOptions &opt, uint32_t nowMs) {
  g.fillScreen(0);
  const int width = g.width();
  const int totalRows = g.height() / ROW_H;
  const bool message = opt.showMessage && opt.message && *opt.message;
  const int depRows = message ? totalRows - 1 : totalRows;

  if (count == 0) {
    const char *txt = "No departures";
    dotfont::drawText(g, (width - dotfont::textWidth(txt)) / 2, ROW_H,
                      txt, opt.color);
  }
  for (int i = 0; i < depRows && i < count; i++) {
    drawRow(g, i, deps[i], opt.color, nowMs);
  }

  if (message) {
    // Scroll in from the right edge until fully gone, then repeat.
    const int msgW = dotfont::textWidth(opt.message);
    const int span = width + msgW;
    const int x = width - (int)((nowMs / MSG_MS_PER_PX) % span);
    dotfont::drawText(g, x, depRows * ROW_H, opt.message, opt.color, 0, width);
  }
}
