// Compile-time hardware settings. Values come from build_flags in
// platformio.ini; the defaults here are fallbacks.
#pragma once

#ifndef PANEL_COUNT
#define PANEL_COUNT 1
#endif
#ifndef BOARD_MAX_BRIGHTNESS
#define BOARD_MAX_BRIGHTNESS 60
#endif
#ifndef BOARD_BIT_DEPTH
#define BOARD_BIT_DEPTH 4  // colour bit planes, 1-6
#endif
#ifndef BOARD_DOUBLE_BUFFER
#define BOARD_DOUBLE_BUFFER false
#endif
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 0  // 0 or 2 only; 1/3 would swap width and height
#endif

constexpr int PANEL_W = 64;
constexpr int PANEL_H = 32;
constexpr int MATRIX_W = PANEL_W * PANEL_COUNT;
constexpr int MATRIX_H = PANEL_H;
