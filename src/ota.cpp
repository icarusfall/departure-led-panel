#include "ota.h"
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>
#include "display.h"
#include "net.h"

static bool enabled = false;

static void drawProgress(unsigned int done, unsigned int total) {
  const int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
  static int lastPct = -1;
  if (pct == lastPct) return;
  lastPct = pct;
  char line[24];
  snprintf(line, sizeof(line), "Updating... %d%%", pct);
  const uint16_t c = dimColor(0xFF8C00);
  displayLines(line, nullptr, nullptr, c);
  // Progress bar along the bottom row.
  matrix.fillRect(0, MATRIX_H - 3, matrix.width() * pct / 100, 2, c);
  matrix.show();
}

void otaBegin(const char *password) {
  if (!password || !*password) {
    Serial.println("OTA disabled: no password set");
    return;
  }
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(password);
  ArduinoOTA.setMdnsEnabled(false);  // net.cpp already runs mDNS
  ArduinoOTA.onStart([] {
    Serial.println("OTA: start");
    displayLines("Updating...");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    esp_task_wdt_reset();  // the upload runs inside otaLoop()
    drawProgress(done, total);
  });
  ArduinoOTA.onEnd([] {
    Serial.println("OTA: done, restarting");
    displayLines("Update done", "Restarting...");
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("OTA error %u\n", e);
    displayLines("Update failed");
    delay(2000);
  });
  ArduinoOTA.begin();
  enabled = true;
  Serial.println("OTA ready");
}

void otaLoop() {
  if (enabled) ArduinoOTA.handle();
}
