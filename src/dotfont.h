// Proportional rendering of the classic Adafruit GFX 5x7 font.
// Letters are trimmed to their inked columns so "i", "l" and "1" take less
// space; digits stay fixed-width so minutes line up in a column.
#pragma once
#include <Adafruit_GFX.h>
#include <limits.h>

namespace dotfont {

constexpr int HEIGHT = 8;  // 7px cap height + 1px for descenders (g j p q y)
constexpr int GAP = 1;     // blank column between glyphs

// Width in pixels of a string as drawn (no trailing gap).
int textWidth(const char *s);

// Draws text with its top-left at (x, y). Pixels outside the horizontal
// clip range [clipL, clipR) are skipped, which lets callers scroll text
// inside a column. Returns the x just past the last glyph.
int drawText(Adafruit_GFX &g, int x, int y, const char *s, uint16_t color,
             int clipL = INT_MIN, int clipR = INT_MAX);

}  // namespace dotfont
