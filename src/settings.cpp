#include "settings.h"
#include <Preferences.h>
#include "modes.h"

Settings settings;
static Preferences prefs;
static const char *NS = "board";

static uint8_t clampPct(uint8_t v) { return v < 5 ? 5 : (v > 100 ? 100 : v); }

void settingsLoad() {
  prefs.begin(NS, true);
  settings.mode = prefs.getUChar("mode", MODE_BUS_NORTH);
  if (settings.mode >= MODE_COUNT) settings.mode = MODE_BUS_NORTH;
  settings.brightness = clampPct(prefs.getUChar("bright", 50));
  settings.color = prefs.getULong("color", 0xFF8C00);  // amber
  settings.showMessage = prefs.getBool("showMsg", true);
  prefs.getString("msg", "Rufus Rules!").toCharArray(
      settings.message, sizeof(settings.message));
  settings.nightEnabled = prefs.getBool("night", false);
  settings.nightStart = prefs.getUShort("nStart", 23 * 60);
  settings.nightEnd = prefs.getUShort("nEnd", 6 * 60 + 30);
  settings.nightOff = prefs.getBool("nOff", false);
  settings.nightBrightness = clampPct(prefs.getUChar("nBright", 15));
  prefs.getString("stopN", "").toCharArray(settings.busStopNorth,
                                           sizeof(settings.busStopNorth));
  prefs.getString("stopS", "").toCharArray(settings.busStopSouth,
                                           sizeof(settings.busStopSouth));
  prefs.getString("tflKey", "").toCharArray(settings.tflKey,
                                            sizeof(settings.tflKey));
  prefs.end();
}

void settingsSave() {
  settings.brightness = clampPct(settings.brightness);
  settings.nightBrightness = clampPct(settings.nightBrightness);
  prefs.begin(NS, false);
  prefs.putUChar("mode", settings.mode);
  prefs.putUChar("bright", settings.brightness);
  prefs.putULong("color", settings.color);
  prefs.putBool("showMsg", settings.showMessage);
  prefs.putString("msg", settings.message);
  prefs.putBool("night", settings.nightEnabled);
  prefs.putUShort("nStart", settings.nightStart);
  prefs.putUShort("nEnd", settings.nightEnd);
  prefs.putBool("nOff", settings.nightOff);
  prefs.putUChar("nBright", settings.nightBrightness);
  prefs.putString("stopN", settings.busStopNorth);
  prefs.putString("stopS", settings.busStopSouth);
  prefs.putString("tflKey", settings.tflKey);
  prefs.end();
}
