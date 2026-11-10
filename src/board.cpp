#include "board.h"
#include <stdio.h>
#include <string.h>
#include "dotfont.h"

static const int ROW_H = 8;          // 7px glyphs + descender row; 4 rows = 32px
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
                    const BoardOptions &opt, uint32_t nowMs) {
  const int y = row * ROW_H;
  const int width = g.width();
  const uint16_t color = opt.color;

  dotfont::drawText(g, 0, y, d.route, color, 0, opt.routeWidth);

  // Minutes, right-aligned. The column is sized for the widest likely value
  // so the destination column doesn't jump around as times change.
  static int minColW = -1;
  if (minColW < 0) minColW = dotfont::textWidth("88 min");
  char mins[12];
  if (d.status[0])
    strlcpy(mins, d.status, sizeof(mins));
  else
    formatMinutes(d.minutes, mins, sizeof(mins));
  dotfont::drawText(g, width - dotfont::textWidth(mins), y, mins, color);

  // Destination, clipped to its column; slides if it doesn't fit.
  const int destL = opt.routeWidth + COL_GAP;
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

int renderMessageRow(Adafruit_GFX &g, const BoardOptions &opt,
                     uint32_t nowMs) {
  const int totalRows = g.height() / ROW_H;
  if (!(opt.showMessage && opt.message && *opt.message)) return totalRows;
  // Scroll in from the right edge until fully gone, then repeat.
  const int width = g.width();
  const int span = width + dotfont::textWidth(opt.message);
  const int x = width - (int)((nowMs / MSG_MS_PER_PX) % span);
  dotfont::drawText(g, x, (totalRows - 1) * ROW_H, opt.message, opt.color, 0,
                    width);
  return totalRows - 1;
}

void renderBoard(Adafruit_GFX &g, const Departure *deps, int count,
                 const BoardOptions &opt, uint32_t nowMs) {
  g.fillScreen(0);
  const int depRows = renderMessageRow(g, opt, nowMs);

  if (count == 0) {
    const char *txt = "No departures";
    dotfont::drawText(g, (g.width() - dotfont::textWidth(txt)) / 2, ROW_H,
                      txt, opt.color);
  }
  for (int i = 0; i < depRows && i < count; i++) {
    drawRow(g, i, deps[i], opt, nowMs);
  }
}

void renderBums(Adafruit_GFX &g, uint16_t color, uint32_t nowMs) {
  static const char *WORD = "bums";
  const int unit = dotfont::textWidth(WORD) + 6;  // word + spacing
  const int width = g.width();
  g.fillScreen(0);
  for (int row = 0; row < g.height() / ROW_H; row++) {
    const int msPerPx = 22 + row * 9;  // each row its own speed
    int shift = (nowMs / msPerPx) % unit;
    if (row % 2) shift = unit - 1 - shift;  // alternate directions
    for (int x = -shift; x < width; x += unit) {
      dotfont::drawText(g, x, row * ROW_H, WORD, color, 0, width);
    }
  }
}
