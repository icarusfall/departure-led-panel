// User settings, persisted in NVS (Preferences) so they survive power cuts.
// Secrets such as the TfL app key live only here, never in the repo.
#pragma once
#include <stdint.h>

struct Settings {
  uint8_t mode;             // see modes.h
  uint8_t brightness;       // 5-100, percent of the hardware cap
  uint32_t color;           // 0xRRGGBB
  bool showMessage;
  char message[101];
  bool nightEnabled;
  uint16_t nightStart;      // minutes since midnight
  uint16_t nightEnd;
  bool nightOff;            // true: blank at night; false: dim
  uint8_t nightBrightness;  // 5-100, used when dimming
  char busStopNorth[16];    // TfL NaPTAN IDs, e.g. 490012345N
  char busStopSouth[16];
  char tflKey[64];
};

extern Settings settings;

void settingsLoad();
void settingsSave();
