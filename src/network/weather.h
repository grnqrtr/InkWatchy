#ifndef WEATHER_H
#define WEATHER_H

#include "defines/defines.h"

struct savedWeatherData {
    uint32_t dt; // time, unix, use strTime
    float temp; // C, depends
    float minTemp; // C, depends
    float pressure; // hPa
    uint8_t humidity; // %
    uint16_t weatherConditionId; // Use weatherConditionIdToStr
    uint8_t cloudsPerc; // % of clouds
    float windSpeed; // m/s
    float windGusts; // m/s
    uint32_t visibility;
    uint8_t pop; // Probability of precipitation in %
    uint32_t sunrise; // time, unix
    uint32_t sunset; // time, unix
};

#define WEATHER_PER_DAY 8
extern bool isWeatherAvailable;
extern savedWeatherData weatherDataDays[MAX_DAYS][WEATHER_PER_DAY]; // Days / Data for these days

void syncWeather();
void saveWeatherData();
void loadWeatherData();

#if DEBUG
void dumpWeather();
#endif

String weatherConditionIdToStr(int weatherCode);

#endif
