#include "wifiLogic.h"
#include "rtcMem.h"

// Keep-wifi-on mode: full sync (NTP + weather + bitcoin) cadence
#define WIFI_KEEP_ON_SYNC_MS 300000 // 5 minutes


void wifiSyncModules()
{
    softStartDelay();
#if WEATHER_INFO
    syncWeather();
#endif
#if BITCOIN_MODULE
    softStartDelay();
    bitcoinSync(1);
#endif
}

void wifiRegular()
{
    debugLog("Launching wifi regular");
    syncNtp();
    syncTimezone();
    wifiSyncModules();
    rM.lastSyncUnix = getUnixTime(timeRTCLocal);
}

void wifiPersistent()
{
    debugLog("Launching");
    while (WiFi.status() == WL_CONNECTED && rM.bat.isCharging == true)
    {
        syncNtp(false);
        int counter = 0;
        while (counter < SYNC_NTP_ON_CHARGING_DELAY && WiFi.status() == WL_CONNECTED && rM.bat.isCharging == true)
        {
            delayTask(1000);
            counter = counter + 1000;
        }
        rM.lastTryUnix = getUnixTime(timeRTCLocal);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        syncNtp(false);
        rM.lastTryUnix = getUnixTime(timeRTCLocal);
    }
}

void wifiKindOfPersistent()
{
    debugLog("Launching");
    syncNtp(false); // Because we are connected to usb for some time now so the drift drifted too much in a bad way
    wifiSyncModules();
    rM.lastSyncUnix = getUnixTime(timeRTCLocal);
}

void turnOnWifiRegular()
{
    // Regular turn on
    createWifiTask(WIFI_CONNECTION_TRIES, wifiRegular, WIFI_PRIORITY_REGULAR);
}

void turnOnWifiPersistent()
{
    // Turn on but keep being connected and sync NTP
    createWifiTask(WIFI_CONNECTION_TRIES_PERSISTENT, wifiPersistent, WIFI_PRIORITY_PERSISTENT);
}

void turnOnWifiKindOfPersistent()
{
    createWifiTask(WIFI_CONNECTION_TRIES_PERSISTENT, wifiKindOfPersistent, WIFI_PRIORITY_PERSISTENT);
}

void wifiKeepOn()
{
    debugLog("Launching wifi keep-on");
    while (rM.keepWifiOn == true && WiFi.status() == WL_CONNECTED)
    {
        syncNtp(false);
        wifiSyncModules();
        rM.lastSyncUnix = getUnixTime(timeRTCLocal);
        // Wait in 1s steps so turning the toggle off reacts within ~1s
        int waitedMs = 0;
        while (waitedMs < WIFI_KEEP_ON_SYNC_MS && rM.keepWifiOn == true && WiFi.status() == WL_CONNECTED)
        {
            delayTask(1000);
            waitedMs = waitedMs + 1000;
        }
    }
    debugLog("Keep-wifi-on loop ended, the task wrapper turns wifi off");
}

void turnOnWifiKeepOn()
{
    // Connect once, then re-sync while the toggle is set
    createWifiTask(WIFI_CONNECTION_TRIES_PERSISTENT, wifiKeepOn, WIFI_PRIORITY_PERSISTENT);
}

void keepWifiAlive()
{
    if (rM.keepWifiOn == true && isWifiTaskCheck() == false)
    {
        debugLog("Keep-wifi-on is set but no task runs, (re)starting");
        turnOnWifiKeepOn();
    }
}

void wifiOnlyNtp() {
    debugLog("Launching wifi only ntp");
    syncNtp();
}

void turnOnWifiNtpOnly() {
    createWifiTask(WIFI_CONNECTION_TRIES, wifiOnlyNtp, WIFI_PRIORITY_REGULAR);
}

void regularSync()
{
    // debugLog("Entering regular sync");
    // debugLog("rM.bat.isCharging:" + BOOL_STR(rM.bat.isCharging));
    #if SYNC_WIFI
    if (rM.bat.isCharging == true && isWifiTaskCheck() == false)
    {
        if (getUnixTime(timeRTCLocal) - rM.lastSyncUnix > SYNC_WIFI_SINCE_SUCC)
        {
            debugLog("Regular sync going on");
            turnOnWifiRegular();
            rM.lastSyncUnix = getUnixTime(timeRTCLocal);
        }
        else if (getUnixTime(timeRTCLocal) - rM.lastTryUnix > SYNC_WIFI_SINCE_FAIL)
        {
            // We dont want persistent without regular first, so we launch a "in between" mode
            if (getUnixTime(timeRTCLocal) - rM.lastSyncUnix > SYNC_WIFI_SINCE_SUCC)
            {
                debugLog("Kind of persistent sync going on");
                turnOnWifiKindOfPersistent();
            }
            else
            {
                debugLog("Persistent sync going on");
                turnOnWifiPersistent();
            }
            rM.lastTryUnix = getUnixTime(timeRTCLocal);
        }
    }
    else
    {
        // debugLog("Not doing regular sync: " + String(getUnixTime(timeRTCLocal) - rM.lastSyncUnix) + " " + BOOL_STR(rM.bat.isCharging));
        if(isWifiTaskCheck() == false && wifiStatusWrap() != WifiOff) {
            debugLog("Turning wifi off after regular sync");
            turnOffWifi();
        }
    }
    #endif
}

#define MIN_RSSI -100
#define MAX_RSSI -40
int getSignalStrength()
{
    int rssi = WiFi.RSSI();
    // debugLog("Pure RSSI:" + String(rssi));
    if (rssi == 0)
    {
        return 0;
    }

    if (rssi < MIN_RSSI)
    {
        rssi = MIN_RSSI;
    }
    else if (rssi > MAX_RSSI)
    {
        rssi = MAX_RSSI;
    }

    int percentage = map(rssi, MIN_RSSI, MAX_RSSI, 0, 100);
    return percentage;
}

wifiStatusSimple wifiStatusWrap()
{
    // TODO: Is asking 2 times time consuming? The answer is no
    // debugLog("Milis before asking time status: " + String(millisBetter()));
    // debugLog("Wifi status: " + wifiStatus());
    wl_status_t status = WiFi.status();
    // debugLog("Milis after asking time status: " + String(millisBetter()));

    if (status == WL_CONNECTED)
    {
        return WifiConnected;
    }
    else
    {
        if (WiFi.getMode() == WIFI_OFF)
        {
            return WifiOff;
        }
        else
        {
            return WifiOn;
        }
    }
}
