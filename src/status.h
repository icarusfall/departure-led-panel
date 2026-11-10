// Small diagnostics shared by the log heartbeat and the status page.
#pragma once
#include <esp_system.h>

inline const char *resetReasonText() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "Power on";
    case ESP_RST_SW: return "Software restart";
    case ESP_RST_PANIC: return "Crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "Watchdog";
    case ESP_RST_BROWNOUT: return "Brownout (power dip)";
    case ESP_RST_USB: return "USB reset";
    case ESP_RST_EXT: return "Reset button";
    case ESP_RST_DEEPSLEEP: return "Deep sleep";
    default: return "Unknown";
  }
}
