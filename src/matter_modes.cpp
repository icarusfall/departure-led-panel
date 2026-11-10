#include "matter_modes.h"
#include <Matter.h>
#include <MatterHelpers.h>
#include <atomic>
#include "modes.h"

using namespace chip::app::Clusters;

// Plugs rather than lights, so "turn off the lights" leaves the board alone.
static MatterOnOffPlugin plugs[MODE_COUNT];
static std::atomic<int> active{0};
static std::atomic<int> requested{-1};
static bool started = false;

// Writes the on/off attribute directly. MatterOnOffPlugin::setOnOff() skips
// the write when its cached state already matches, which can leave the
// stored attribute (restored from flash at boot) out of step.
static void setStore(uint8_t i, bool on) {
  esp_matter_attr_val_t v = esp_matter_invalid(NULL);
  if (!plugs[i].getAttributeVal(OnOff::Id, OnOff::Attributes::OnOff::Id, &v))
    return;
  if (v.val.b == on) return;
  v.val.b = on;
  plugs[i].updateAttributeVal(OnOff::Id, OnOff::Attributes::OnOff::Id, &v);
}

// Runs in the Matter task, for commands from Google Home and for our own
// setStore() writes. Turning a mode on turns the others off; switching the
// active mode off is refused, so one mode is always on.
static bool onChange(uint8_t i, bool on) {
  if (!on) return (int)i != active.load();
  if ((int)i != active.load()) {
    active = i;
    requested = i;
    Serial.printf("Matter: mode -> %s\n", MODES[i].title);
  }
  for (uint8_t j = 0; j < MODE_COUNT; j++) {
    if (j != i) setStore(j, false);
  }
  return true;
}

void matterModesBegin(uint8_t mode) {
  active = mode;
  // Bluetooth commissioning off: the board joins Wi-Fi itself, and BLE's
  // RAM doesn't fit alongside the display and TLS. Pairing then happens
  // on-network.
  Matter.selectNetwork(MATTER_NETWORK_WIFI, true);
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    plugs[i].begin(i == mode);
    plugs[i].onChangeOnOff([i](bool on) { return onChange(i, on); });
  }
  Matter.begin();
  started = true;
  // The stored states may be from an earlier run; make them match `mode`.
  for (uint8_t i = 0; i < MODE_COUNT; i++) setStore(i, i == mode);
  requested = -1;
  Serial.printf("Matter started. Commissioned: %s\n",
                Matter.isDeviceCommissioned() ? "yes" : "no");
  if (!Matter.isDeviceCommissioned())
    Serial.printf("Pairing code: %s\n", matterModesPairingCode().c_str());
}

void matterModesLoop() {
  if (started) matterRestartIfNoFabric();  // removed from Google Home
}

int matterModesTakeRequest() { return requested.exchange(-1); }

void matterModesShow(uint8_t mode) {
  if (!started || (int)mode == active.load()) return;
  setStore(mode, true);  // onChange() switches the others off
}

bool matterModesCommissioned() {
  return started && Matter.isDeviceCommissioned();
}

String matterModesPairingCode() {
  String c = Matter.getManualPairingCode();
  // 11 digits shown as XXXX-XXX-XXXX, the way Google Home asks for it.
  if (c.length() == 11) c = c.substring(0, 4) + "-" + c.substring(4, 7) + "-" + c.substring(7);
  return c;
}

String matterModesQrUrl() { return Matter.getOnboardingQRCodeUrl(); }

void matterModesUnpair() {
  if (started) Matter.decommission();
}
