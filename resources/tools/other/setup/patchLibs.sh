#!/bin/bash

source globalFunctions.sh

pio_env=$(get_pio_env ../../.vscode/launch.json)

rtc_path="../../.pio/libdeps/$pio_env/Rtc_Pcf8563/src/Rtc_Pcf8563.h"

wrong_st="#define RTCC_VERSION  \"Pcf8563 v1.0.3\""

new_str="#define RTCC_VERSION  (char*)\"Pcf8563 v1.0.3\""

sed -i "s/^${wrong_st}.*/$new_str/" "$rtc_path"

# With SYNC_PROXY set in src/defines/config.h, point the open-meteo lib at
# the plaintext phone proxy (same paths, phone host). Otherwise restore the
# stock http URLs. Idempotent both ways.
om_path="../../.pio/libdeps/$pio_env/open-meteo-arduino/src/OpenMeteo.cpp"
# Drop any leftover diagnostic line from earlier debugging.
sed -i '/SYNC_DIAG weather HTTP/d' "$om_path"
# Take only the first quoted string so trailing // comments never glue on.
proxy_base=$(grep -E '^#define SYNC_PROXY_BASE' ../../src/defines/config.h | grep -o '"[^"]*"' | head -1 | tr -d '"')
proxy_base=${proxy_base:-http://192.168.43.1:8080}
# Normalize first: a previous proxy base (e.g. after the phone's DHCP IP
# changed) matches neither the stock hosts nor the current base, so reset
# any proxy-style URL to stock before applying the current setting.
sed -i -E "s|http://[^\" ]*/v1/forecast|http://api.open-meteo.com/v1/forecast|g; s|http://[^\" ]*/v1/air-quality|http://air-quality-api.open-meteo.com/v1/air-quality|g" "$om_path"
if grep -qE '^#define SYNC_PROXY 1( |$)' ../../src/defines/config.h; then
    # Same /v1/... paths, phone host. The proxy routes /v1/forecast to the
    # forecast API and /v1/air-quality to the air-quality API.
    sed -i "s|http://api.open-meteo.com|${proxy_base}|g; s|https://api.open-meteo.com|${proxy_base}|g; s|http://air-quality-api.open-meteo.com|${proxy_base}|g; s|https://air-quality-api.open-meteo.com|${proxy_base}|g" "$om_path"
else
    sed -i "s|${proxy_base}/v1/forecast|http://api.open-meteo.com/v1/forecast|g; s|${proxy_base}/v1/air-quality|http://air-quality-api.open-meteo.com/v1/air-quality|g" "$om_path"
fi
