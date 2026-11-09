// On-device settings page (http://departures.local).
#pragma once

// onModeChange is called after the mode is changed from the page.
void webBegin(void (*onModeChange)());
void webLoop();
