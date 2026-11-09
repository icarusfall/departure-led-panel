#include "dotfont.h"

// The GFX library's built-in font table: 5 column bytes per character,
// bit 0 = top row. It is declared static, so including it here gives this
// file its own copy to read from.
#include <glcdfont.c>

namespace dotfont {

static const int SPACE_W = 2;

// First and last inked column of a glyph (0..4).
static void glyphSpan(uint8_t c, int &first, int &last) {
  if (c >= '0' && c <= '9') {  // tabular digits
    first = 0;
    last = 4;
    return;
  }
  first = -1;
  last = -1;
  for (int i = 0; i < 5; i++) {
    if (pgm_read_byte(&font[c * 5 + i])) {
      if (first < 0) first = i;
      last = i;
    }
  }
}

static int glyphWidth(uint8_t c) {
  int first, last;
  glyphSpan(c, first, last);
  return first < 0 ? SPACE_W : last - first + 1;
}

int textWidth(const char *s) {
  int w = 0;
  for (; *s; s++) w += glyphWidth((uint8_t)*s) + GAP;
  return w > 0 ? w - GAP : 0;
}

int drawText(Adafruit_GFX &g, int x, int y, const char *s, uint16_t color,
             int clipL, int clipR) {
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    int first, last;
    glyphSpan(c, first, last);
    if (first < 0) {
      x += SPACE_W + GAP;
      continue;
    }
    for (int col = first; col <= last; col++, x++) {
      if (x < clipL || x >= clipR) continue;
      uint8_t bits = pgm_read_byte(&font[c * 5 + col]);
      for (int row = 0; row < HEIGHT; row++) {
        if (bits & (1 << row)) g.drawPixel(x, y + row, color);
      }
    }
    x += GAP;
  }
  return x - GAP;
}

}  // namespace dotfont
