// Over-the-air firmware updates (ArduinoOTA / espota), password-protected.
// Disabled until an OTA password is set on the settings page.
#pragma once

void otaBegin(const char *password);
void otaLoop();
