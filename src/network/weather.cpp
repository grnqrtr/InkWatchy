#include "weather.h"
#include "rtcMem.h"

#if WEATHER_INFO
#define ADD_DAY_UNIX 86400

#if SYNC_PROXY
#define COMPACT_WEATHER_API_LINK "&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation_probability,weather_code,pressure_msl,cloud_cover,visibility,wind_speed_10m,wind_direction_10m,wind_gusts_10m,is_day,wet_bulb_temperature_2m,cape,uv_index,uv_index_clear_sky&daily=sunrise,sunset,sunshine_duration&timeformat=unixtime&timezone=auto"

template <typename T>
bool parseCompactWeatherArray(const String &payload, const char *key, T *values, size_t count)
{
  String marker = String(key) + "=[";
  int keyIndex = payload.indexOf(marker);
  if (keyIndex < 0)
  {
    debugLog("Missing compact weather field: " + String(key));
    return false;
  }

  const char *cursor = payload.c_str() + keyIndex + marker.length();

  for (size_t i = 0; i < count; i++)
  {
    while (*cursor == ' ')
    {
      cursor++;
    }

    if (strncmp(cursor, "null", 4) == 0)
    {
      values[i] = 0;
      cursor = cursor + 4;
    }
    else
    {
      char *end = nullptr;
      double value = strtod(cursor, &end);
      if (end == cursor)
      {
        debugLog("Invalid compact weather value: " + String(key));
        return false;
      }
      values[i] = static_cast<T>(value);
      cursor = end;
    }

    if (i + 1 < count)
    {
      if (*cursor != ',')
      {
        debugLog("Short compact weather field: " + String(key));
        return false;
      }
      cursor++;
    }
  }
  if (*cursor != ']')
  {
    debugLog("Invalid compact weather array end: " + String(key));
    return false;
  }
  return true;
}

String compactWeatherDate(uint64_t unixTime)
{
  char date[11];
  snprintf(date, sizeof(date), "%04d-%02d-%02d", year(unixTime), month(unixTime), day(unixTime));
  return String(date);
}

bool getCompactHourlyForecast(OM_HourlyForecast *forecast, float latitude, float longitude, uint64_t unixTime)
{
  memset(forecast, 0, sizeof(*forecast));
  String date = compactWeatherDate(unixTime);
  String url = String(SYNC_PROXY_BASE) + "/v1/forecast?watchy_compact=1&latitude=" + String(latitude) + "&longitude=" + String(longitude) + COMPACT_WEATHER_API_LINK + "&start_date=" + date + "&end_date=" + date;
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(17500);
  http.setConnectTimeout(17500);
  if (http.begin(client, url) == false)
  {
    debugLog("Failed to begin compact weather request");
    return false;
  }

  int httpCode = http.GET();
  debugLog("Compact weather HTTP code: " + String(httpCode));
  if (httpCode != HTTP_CODE_OK)
  {
    http.end();
    return false;
  }

  WiFiClient &stream = http.getStream();
  stream.setTimeout(17500);
  uint32_t parsedFields = 0;
  const uint32_t requiredFields = (1UL << 21) - 1;
  bool status = true;
  uint64_t readDeadline = millisBetter() + 20000;

  while (status == true && parsedFields != requiredFields && millisBetter() < readDeadline)
  {
    if (stream.available() == 0)
    {
      if (http.connected() == false)
      {
        break;
      }
      delayTask(10);
      continue;
    }

    String line = stream.readStringUntil('\n');
    if (line.endsWith("\r"))
    {
      line.remove(line.length() - 1);
    }

#define PARSE_HOURLY(member, field, bit)                                                                                 \
  if (line.startsWith("hourly." field "=["))                                                                           \
  {                                                                                                                      \
    status = parseCompactWeatherArray(line, "hourly." field, forecast->member, OM_WEATHER_MAX_HOURS);                  \
    if (status == true)                                                                                                  \
    {                                                                                                                    \
      parsedFields |= (1UL << bit);                                                                                      \
    }                                                                                                                    \
  }
    PARSE_HOURLY(hourly_time, "time", 0);
    PARSE_HOURLY(temp, "temperature_2m", 1);
    PARSE_HOURLY(humidity, "relative_humidity_2m", 2);
    PARSE_HOURLY(apparent_temp, "apparent_temperature", 3);
    PARSE_HOURLY(precipitation, "precipitation_probability", 4);
    PARSE_HOURLY(weather_code, "weather_code", 5);
    PARSE_HOURLY(pressure, "pressure_msl", 6);
    PARSE_HOURLY(cloud_cover, "cloud_cover", 7);
    PARSE_HOURLY(visibility, "visibility", 8);
    PARSE_HOURLY(wind_speed, "wind_speed_10m", 9);
    PARSE_HOURLY(wind_deg, "wind_direction_10m", 10);
    PARSE_HOURLY(wind_gust, "wind_gusts_10m", 11);
    PARSE_HOURLY(is_day, "is_day", 12);
    PARSE_HOURLY(wet_bulb_temperature_2m, "wet_bulb_temperature_2m", 13);
    PARSE_HOURLY(cape, "cape", 14);
    PARSE_HOURLY(uv_index, "uv_index", 15);
    PARSE_HOURLY(uv_index_clear_sky, "uv_index_clear_sky", 16);
#undef PARSE_HOURLY

#define PARSE_DAILY(member, field, bit)                                                                                  \
  if (line.startsWith("daily." field "=["))                                                                            \
  {                                                                                                                      \
    status = parseCompactWeatherArray(line, "daily." field, forecast->member, OM_WEATHER_MAX_DAYS);                    \
    if (status == true)                                                                                                  \
    {                                                                                                                    \
      parsedFields |= (1UL << bit);                                                                                      \
    }                                                                                                                    \
  }
    PARSE_DAILY(daily_time, "time", 17);
    PARSE_DAILY(sunrise, "sunrise", 18);
    PARSE_DAILY(sunset, "sunset", 19);
    PARSE_DAILY(sunshine_duration, "sunshine_duration", 20);
#undef PARSE_DAILY
  }

  http.end();
  if (status == false || parsedFields != requiredFields)
  {
    debugLog("Incomplete compact weather response, fields: " + String(parsedFields, HEX));
    return false;
  }

  return true;
}

#if WEATHER_AIR_ENABLED
bool getCompactAirQualityForecast(OM_AirQualityForecast *forecast, float latitude, float longitude, uint64_t unixTime)
{
  String date = compactWeatherDate(unixTime);
  String url = String(SYNC_PROXY_BASE) + "/v1/air-quality?watchy_compact=1&latitude=" + String(latitude) + "&longitude=" + String(longitude) + AIR_HOURLY_API_LINK + "&start_date=" + date + "&end_date=" + date;
  String payload = getStringRequest(url);
  bool status = true;

#define PARSE_AIR(member, field) status = parseCompactWeatherArray(payload, "hourly." field, forecast->member, OM_AIR_QUALITY_MAX_HOURS) && status
  PARSE_AIR(hourly_time, "time");
  PARSE_AIR(european_aqi, "european_aqi");
  PARSE_AIR(us_aqi, "us_aqi");
  PARSE_AIR(european_aqi_pm2_5, "european_aqi_pm2_5");
  PARSE_AIR(european_aqi_pm10, "european_aqi_pm10");
  PARSE_AIR(european_aqi_nitrogen_dioxide, "european_aqi_nitrogen_dioxide");
  PARSE_AIR(european_aqi_ozone, "european_aqi_ozone");
  PARSE_AIR(european_aqi_sulphur_dioxide, "european_aqi_sulphur_dioxide");
  PARSE_AIR(pm2_5, "pm2_5");
  PARSE_AIR(pm10, "pm10");
  PARSE_AIR(carbon_monoxide, "carbon_monoxide");
  PARSE_AIR(carbon_dioxide, "carbon_dioxide");
  PARSE_AIR(nitrogen_dioxide, "nitrogen_dioxide");
  PARSE_AIR(sulphur_dioxide, "sulphur_dioxide");
  PARSE_AIR(ozone, "ozone");
  PARSE_AIR(aerosol_optical_depth, "aerosol_optical_depth");
  PARSE_AIR(dust, "dust");
  PARSE_AIR(methane, "methane");
  PARSE_AIR(formaldehyde, "formaldehyde");
  PARSE_AIR(glyoxal, "glyoxal");
  PARSE_AIR(nitrogen_monoxide, "nitrogen_monoxide");
  PARSE_AIR(peroxyacyl_nitrates, "peroxyacyl_nitrates");
#undef PARSE_AIR

  return status;
}
#endif
#endif

// http://api.open-meteo.com/v1/forecast?latitude=53.543082&longitude=9.994695&hourly=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation_probability,weather_code,pressure_msl,cloud_cover,visibility,wind_speed_10m,wind_direction_10m,wind_gusts_10m,is_day&daily=sunrise,sunset&timeformat=unixtime&timezone=auto&forecast_days=16
void syncWeather()
{
  if (strlen(WEATHER_LONGTIT) == 0 || strlen(WEATHER_LATIT) == 0)
  {
    debugLog("Weather location not supplied, skipping");
    return;
  }

  debugLog("Syncing weather");
  OM_HourlyForecast *forecast = new OM_HourlyForecast;
  if (fsFileExists(WEATHER_DIR) == false)
  {
    fsCreateDir(WEATHER_DIR);
  }
  if (fsFileExists(WEATHER_HOURLY_DIR) == false)
  {
    fsCreateDir(WEATHER_HOURLY_DIR);
  }

  for (u8_t iw = 0; iw < MAX_WEATHER_DAYS; iw++)
  {
    bool status = false;
    uint64_t unixTimeWeat = simplifyUnix(getUnixTime(timeRTCLocal) + (ADD_DAY_UNIX * iw));
    String currentDayDate = unixToDate(unixTimeWeat);
    String weatherName = String(unixTimeWeat);
    String weatherPath = String(WEATHER_HOURLY_DIR) + "/" + weatherName;
    String stagingName = weatherName + ".new";
    String stagingPath = String(WEATHER_HOURLY_DIR) + "/" + stagingName;
    String backupPath = weatherPath + ".bak";

    if (LittleFS.exists(weatherPath) == false && LittleFS.exists(backupPath))
    {
      if (LittleFS.rename(backupPath, weatherPath) == false)
      {
        debugLog("Failed to recover the previous weather snapshot");
      }
    }
    for (u8_t i = 0; i < WEATHER_TRIES; i++)
    {
      debugLog("Trying to get weather for day: " + String(currentDayDate));
#if SYNC_PROXY
      status = getCompactHourlyForecast(forecast, String(WEATHER_LATIT).toFloat(), String(WEATHER_LONGTIT).toFloat(), unixTimeWeat);
#else
      status = getHourlyForecast(forecast, String(WEATHER_LATIT).toFloat(), String(WEATHER_LONGTIT).toFloat(), unixTimeWeat);
#endif
      if (status == true)
      {
        break;
      }
      else
      {
        debugLog("Failed to get weather, retrying...");
        delayTask(2000);
      }
      softStartDelay();
    }
    softStartDelay();

    if (status == false)
    {
      debugLog("Failed to get weather... bye");
      delete forecast;
      return;
    }

#if DEBUG && true == false
    for (int i = 0; i < OM_WEATHER_MAX_HOURS; ++i)
    {
      debugLog("Hour " + String(i) + ": ");
      debugLog("Time: " + String(forecast->hourly_time[i]));
      debugLog(", Temperature: " + String(forecast->temp[i]));
      debugLog(", Humidity: " + String(forecast->humidity[i]));
      debugLog(", Apparent Temperature: " + String(forecast->apparent_temp[i]));
      debugLog(", Pressure: " + String(forecast->pressure[i]));
      debugLog(", Pop: " + String(forecast->precipitation[i]));
      debugLog(", Cloud Cover: " + String(forecast->cloud_cover[i]));
      debugLog(", Visibility: " + String(forecast->visibility[i]));
      debugLog(", Wind Speed: " + String(forecast->wind_speed[i]));
      debugLog(", Wind Direction: " + String(forecast->wind_deg[i]));
      debugLog(", Wind Gust: " + String(forecast->wind_gust[i]));
      debugLog(", Weather Code: " + String(forecast->weather_code[i]));
      debugLog(", is Day: " + String(forecast->is_day[i]));
      debugLog(", Wet Bulb Temperature: " + String(forecast->wet_bulb_temperature_2m[i]));
      debugLog(", CAPE: " + String(forecast->cape[i]));
      debugLog(", UV Index: " + String(forecast->uv_index[i]));
      debugLog(", UV Index Clear Sky: " + String(forecast->uv_index_clear_sky[i]));
      debugLog(", Dew Point: " + String(forecast->dew_point[i]));
      debugLog(", Snow Depth: " + String(forecast->snow_depth[i]));
      // debugLog(", Vapour Pressure Deficit: " + String(forecast->vapour_pressure_deficit[i]));
      // debugLog(", Evapotranspiration: " + String(forecast->evapotranspiration[i]));
      debugLog(", Lifted Index: " + String(forecast->lifted_index[i]));
      debugLog(", Convective Inhibition: " + String(forecast->convective_inhibition[i]));
      debugLog(", Precipitation Amount: " + String(forecast->precipitation_amount[i]));
      debugLog(", Terrestrial Radiation: " + String(forecast->terrestrial_radiation[i]));
      // debugLog(", Global Tilted Irradiance: " + String(forecast->global_tilted_irradiance[i]));
      // debugLog(", Direct Normal Irradiance: " + String(forecast->direct_normal_irradiance[i]));
      debugLog(", Diffuse Radiation: " + String(forecast->diffuse_radiation[i]));
      debugLog(", Direct Radiation: " + String(forecast->direct_radiation[i]));
      // debugLog(", Shortwave Radiation: " + String(forecast->shortwave_radiation[i]));
      // debugLog(", Total Column Water Vapour: " + String(forecast->total_water_vapour[i]));
    }
#endif

    LittleFS.remove(stagingPath);
    if (fsSetBlob(stagingName, (uint8_t *)forecast, sizeof(OM_HourlyForecast), String(WEATHER_HOURLY_DIR) + "/") == false)
    {
      debugLog("Failed to set weather " + String(currentDayDate));
      delete forecast;
      return;
    }

    // Keep the old file until the complete replacement has been written. File
    // renames avoid LittleFS directory-rename path aliasing during cleanup.
    bool hadWeather = LittleFS.exists(weatherPath);
    LittleFS.remove(backupPath);
    if (hadWeather && LittleFS.rename(weatherPath, backupPath) == false)
    {
      debugLog("Failed to back up the previous weather snapshot");
      LittleFS.remove(stagingPath);
      delete forecast;
      return;
    }
    if (LittleFS.rename(stagingPath, weatherPath) == false)
    {
      debugLog("Failed to activate the new weather snapshot");
      if (hadWeather && LittleFS.rename(backupPath, weatherPath) == false)
      {
        debugLog("Failed to restore the previous weather snapshot");
      }
      LittleFS.remove(stagingPath);
      delete forecast;
      return;
    }
    LittleFS.remove(backupPath);
  }
  delete forecast;
  rM.isWeatherDataNew = true;
  debugLog("Weather snapshot replaced: " + String(sizeof(OM_HourlyForecast)) + " bytes");

#if WEATHER_AIR_ENABLED
  OM_AirQualityForecast *airForecast = new OM_AirQualityForecast;
  removeDir(AIR_QUALITY_DIR);
  fsCreateDir(AIR_QUALITY_DIR);
  fsCreateDir(AIR_QUALITY_HOURLY_DIR);

  for (u8_t iw = 0; iw < MAX_AIR_DAYS; iw++)
  {
    uint64_t unixTimeAir = simplifyUnix(getUnixTime(timeRTCLocal) + (ADD_DAY_UNIX * iw));
    String currentDayDate = unixToDate(unixTimeAir);
    bool status = false;
    for (u8_t i = 0; i < WEATHER_TRIES; i++)
    {
      debugLog("Trying to get air quality for day: " + String(currentDayDate));
#if SYNC_PROXY
      status = getCompactAirQualityForecast(airForecast, String(WEATHER_LATIT).toFloat(), String(WEATHER_LONGTIT).toFloat(), unixTimeAir);
#else
      status = getAirQualityForecast(airForecast, String(WEATHER_LATIT).toFloat(), String(WEATHER_LONGTIT).toFloat(), unixTimeAir);
#endif
      if (status == true)
      {
        break;
      }
      else
      {
        debugLog("Failed to get air quality, retrying...");
        delayTask(2000);
      }
      softStartDelay();
    }
    softStartDelay();

    if (status == false)
    {
      debugLog("Failed to get air quality... bye");
      delete airForecast;
      return;
    }
#if DEBUG && true == false
    for (size_t i = 0; i < OM_AIR_QUALITY_MAX_HOURS; i++)
    {
      debugLog(", European AQI: " + String(airForecast->european_aqi[i]));
      debugLog(", US AQI: " + String(airForecast->us_aqi[i]));
      debugLog(", European AQI PM2.5: " + String(airForecast->european_aqi_pm2_5[i]));
      debugLog(", European AQI PM10: " + String(airForecast->european_aqi_pm10[i]));
      debugLog(", European AQI Nitrogen Dioxide: " + String(airForecast->european_aqi_nitrogen_dioxide[i]));
      debugLog(", European AQI Ozone: " + String(airForecast->european_aqi_ozone[i]));
      debugLog(", European AQI Sulphur Dioxide: " + String(airForecast->european_aqi_sulphur_dioxide[i]));
      debugLog(", PM2.5: " + String(airForecast->pm2_5[i]));
      debugLog(", PM10: " + String(airForecast->pm10[i]));
      debugLog(", Carbon Monoxide: " + String(airForecast->carbon_monoxide[i]));
      debugLog(", Carbon Dioxide: " + String(airForecast->carbon_dioxide[i]));
      debugLog(", Nitrogen Dioxide: " + String(airForecast->nitrogen_dioxide[i]));
      debugLog(", Sulphur Dioxide: " + String(airForecast->sulphur_dioxide[i]));
      debugLog(", Ozone: " + String(airForecast->ozone[i]));
      debugLog(", Aerosol Optical Depth: " + String(airForecast->aerosol_optical_depth[i]));
      debugLog(", Dust: " + String(airForecast->dust[i]));
      debugLog(", Methane: " + String(airForecast->methane[i]));
      debugLog(", Formaldehyde: " + String(airForecast->formaldehyde[i]));
      debugLog(", Glyoxal: " + String(airForecast->glyoxal[i]));
      // debugLog(", Sea Salt Aerosol: " + String(airForecast->sea_salt_aerosol[i]));
      debugLog(", Nitrogen Monoxide: " + String(airForecast->nitrogen_monoxide[i]));
      debugLog(", Peroxyacyl Nitrates: " + String(airForecast->peroxyacyl_nitrates[i]));
    }
#endif
    if (fsSetBlob(String(unixTimeAir), (uint8_t *)airForecast, sizeof(OM_AirQualityForecast), String(AIR_QUALITY_HOURLY_DIR) + "/") == false)
    {
      debugLog("Failed to set air quality: " + String(currentDayDate));
    }
  }
  delete airForecast;
#endif
  debugLog("Finished syncing weather/airquality");
}

#if DEBUG
void dumpWeather()
{
}
#endif

String weatherConditionIdToStr(u8_t weatherCode)
{
  switch (weatherCode)
  {
  case 0:
    return "Clear sky";
  case 1:
    return "Mainly clear";
  case 2:
    return "Partly cloudy";
  case 3:
    return "Overcast";
  case 45:
    return "Fog";
  case 48:
    return "Depositing rime fog";
  case 51:
    return "Drizzle: Light intensity";
  case 53:
    return "Drizzle: Moderate intensity";
  case 55:
    return "Drizzle: Dense intensity";
  case 56:
    return "Freezing Drizzle: Light intensity";
  case 57:
    return "Freezing Drizzle: Dense intensity";
  case 61:
    return "Rain: Slight intensity";
  case 63:
    return "Rain: Moderate intensity";
  case 65:
    return "Rain: Heavy intensity";
  case 66:
    return "Freezing Rain: Light intensity";
  case 67:
    return "Freezing Rain: Heavy intensity";
  case 71:
    return "Snow fall: Slight intensity";
  case 73:
    return "Snow fall: Moderate intensity";
  case 75:
    return "Snow fall: Heavy intensity";
  case 77:
    return "Snow grains";
  case 80:
    return "Rain showers: Slight intensity";
  case 81:
    return "Rain showers: Moderate intensity";
  case 82:
    return "Rain showers: Violent intensity";
  case 85:
    return "Snow showers: Slight intensity";
  case 86:
    return "Snow showers: Heavy intensity";
  case 95:
    return "Thunderstorm: Slight or moderate";
  case 96:
    return "Thunderstorm with slight hail";
  case 99:
    return "Thunderstorm with heavy hail";
  default:
    return "Unknown weather condition";
  }
}

#endif
