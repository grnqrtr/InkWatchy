#!/bin/bash

source globalFunctions.sh

pio_env=$(get_pio_env ../../.vscode/launch.json)

rtc_path="../../.pio/libdeps/$pio_env/Rtc_Pcf8563/src/Rtc_Pcf8563.h"

wrong_st="#define RTCC_VERSION  \"Pcf8563 v1.0.3\""

new_str="#define RTCC_VERSION  (char*)\"Pcf8563 v1.0.3\""

sed -i "s/^${wrong_st}.*/$new_str/" "$rtc_path"

# open-meteo serves http->https redirects which the arduino lib does not
# follow (empty payload -> "Failed to get weather"). Force https; the
# core runs TLS insecure when no CA is given, and the lib sets 17.5s
# timeouts, so this works with modem-sleep power saving.
om_path="../../.pio/libdeps/$pio_env/open-meteo-arduino/src/OpenMeteo.cpp"
sed -i "s|http://api.open-meteo.com|https://api.open-meteo.com|; s|http://air-quality-api.open-meteo.com|https://air-quality-api.open-meteo.com|" "$om_path"