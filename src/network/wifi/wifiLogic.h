#ifndef WIFI_LOGIC_H
#define WIFI_LOGIC_H

#include "defines/defines.h"

void turnOnWifiRegular();
void turnOnWifiPersistent();
int getSignalStrength();
void regularSync();

#endif
