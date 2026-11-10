// Matter prototype (milestone 7, step 1). Built only by the matter_proto
// env; the main firmware excludes src/proto/.
//
// Joins the Wi-Fi saved by the main firmware, starts Matter with BLE
// commissioning off (on-network commissioning), and exposes six on/off
// plug endpoints, one per display mode, kept mutually exclusive. Logs heap
// use and the pairing code to serial. No display output.

#include <Arduino.h>
#include <Matter.h>
#include <WiFi.h>

static const uint8_t N = 6;
static const char *NAMES[N] = {"Buses north", "Buses south",
                               "Streatham north", "Streatham Common north",
                               "Streatham Common south", "Secret"};
static MatterOnOffPlugin plugs[N];
static int active = 0;

static void logHeap(const char *when) {
  Serial.printf("[heap] %-22s internal free=%uK (min %uK) psram free=%uK\n",
                when, (unsigned)(ESP.getFreeHeap() / 1024),
                (unsigned)(ESP.getMinFreeHeap() / 1024),
                (unsigned)(ESP.getFreePsram() / 1024));
}

// Turning one mode on turns the others off; turning the active one off is
// refused, so exactly one mode is always on.
static bool onChange(uint8_t i, bool on) {
  Serial.printf("Matter: '%s' -> %s\n", NAMES[i], on ? "ON" : "OFF");
  if (!on) return i != active;  // can't switch the active mode off
  active = i;
  for (uint8_t j = 0; j < N; j++) {
    if (j != i) plugs[j].setOnOff(false);
  }
  Serial.printf("Active mode: %s\n", NAMES[active]);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n=== Matter prototype ===");
  logHeap("boot");

  // Bluetooth (CHIPoBLE) commissioning: Google Home sends the Wi-Fi
  // credentials over BLE and Matter joins Wi-Fi itself, so no WiFi.begin().
  // BLE memory is released once commissioning completes.
  Serial.printf("BLE commissioning compiled in: %s\n",
                Matter.isBLECommissioningEnabled() ? "yes" : "NO");
  Matter.selectNetwork(MATTER_NETWORK_WIFI);
  for (uint8_t i = 0; i < N; i++) {
    plugs[i].begin(i == active);
    plugs[i].onChangeOnOff([i](bool on) { return onChange(i, on); });
  }
  Matter.begin();
  logHeap("after Matter.begin");

  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Not commissioned yet.");
    Serial.printf("Manual pairing code: %s\n",
                  Matter.getManualPairingCode().c_str());
    Serial.printf("QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
  } else {
    Serial.println("Already commissioned.");
  }
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 30000) {
    last = millis();
    Serial.printf("[%lus] commissioned=%d wifi=%s %s active='%s' ",
                  (unsigned long)(millis() / 1000),
                  (int)Matter.isDeviceCommissioned(),
                  WiFi.status() == WL_CONNECTED ? "up" : "down",
                  WiFi.localIP().toString().c_str(), NAMES[active]);
    logHeap("");
    if (!Matter.isDeviceCommissioned()) {
      Serial.printf("Manual pairing code: %s\n",
                    Matter.getManualPairingCode().c_str());
    }
  }
  delay(50);
}
