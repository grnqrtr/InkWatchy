#pragma once

#include "defines.h"

void turnOnWifiRegular();
void turnOnWifiPersistent();
void turnOnWifiNtpOnly();
void turnOnWifiKeepOn();
// Restarts the keep-on task if the toggle is set but no task runs
// (e.g. after wake from sleep or a drop). Safe to call every loop.
void keepWifiAlive();
int getSignalStrength();
void regularSync();

