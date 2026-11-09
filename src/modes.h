// Display modes. Exactly one is active at a time; later each becomes a
// Matter on/off endpoint for voice control.
#pragma once
#include <stdint.h>

enum Mode : uint8_t {
  MODE_BUS_NORTH = 0,
  MODE_BUS_SOUTH,
  MODE_STREATHAM_NORTH,
  MODE_STC_NORTH,
  MODE_STC_SOUTH,
  MODE_BUMS,
  MODE_COUNT
};

struct ModeInfo {
  const char *title;   // shown on the panel when the mode changes
  const char *button;  // label on the settings page
  const char *place;   // first line of the train placeholder
  const char *dir;     // second line of the train placeholder
};

static const ModeInfo MODES[MODE_COUNT] = {
    {"Buses north", "Buses north - Conyers Rd", "Conyers Road", "Northbound"},
    {"Buses south", "Buses south - Conyers Rd", "Conyers Road", "Southbound"},
    {"Streatham north", "Trains north - Streatham", "Streatham", "Northbound"},
    {"Streatham Common north", "Trains north - Streatham Common",
     "Streatham Common", "Northbound"},
    {"Streatham Common south", "Trains south - Streatham Common",
     "Streatham Common", "Southbound"},
    {"bums", "Secret", "", ""},
};

inline bool modeIsBus(uint8_t m) {
  return m == MODE_BUS_NORTH || m == MODE_BUS_SOUTH;
}
inline bool modeIsTrain(uint8_t m) {
  return m >= MODE_STREATHAM_NORTH && m <= MODE_STC_SOUTH;
}
