// Made-up departures for testing the layout before live data (milestone 4).
// Times count down quickly so you can watch the board reorder.
#pragma once
#include "board.h"

constexpr int FAKE_MAX = 8;

void fakeFeedBegin();
// Advances the simulation; returns true if the list changed.
bool fakeFeedUpdate(uint32_t nowMs);
const Departure *fakeFeedList();
int fakeFeedCount();
