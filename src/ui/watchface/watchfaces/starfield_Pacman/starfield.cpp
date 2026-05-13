#include "starfield.h"
#include "rtcMem.h"

#if WATCHFACE_NATIVE_STARFIELD

#if !(GSR_WATCHFACES && GSR_STARFIELD)
#define NATIVE_STARFIELD_SUPPLIES_GSR_HELPERS 1
#undef GSR_WATCHFACES
#undef GSR_STARFIELD
#define GSR_WATCHFACES 1
#define GSR_STARFIELD 1
#endif

#include "../gsr/starfield/Dusk2Dawn.h"
#include "../gsr/starfield/moonPhaser.h"
#include "../gsr/starfield/Seven_Segment10pt7b.h"
#include "../gsr/starfield/DSEG7_Classic_Bold_53.h"
#include "../gsr/starfield/icons.h"

#if NATIVE_STARFIELD_SUPPLIES_GSR_HELPERS
#include "../gsr/starfield/moonPhaser.cpp"
#endif

#define STARFIELD_MODULE_X 7
#define STARFIELD_MODULE_Y 160
#define STARFIELD_MODULE_W 177
#define STARFIELD_MODULE_H 37

#define STARFIELD_SUN_X 110
#define STARFIELD_SUN_TOP_Y 72
#define STARFIELD_SUN_BOTTOM_Y 132

static moonPhaser moonP;
static uint8_t lastMoonDay = 255;
static uint8_t lastSunMinute = 255;
static uint8_t lastSunHour = 255;

static bool use24HourClock()
{
    return !WATCHFACE_12H;
}

static int calendarYear(tmElements_t timeEl)
{
    return timeEl.Year + 1970;
}

static String shortUpper(String value, uint8_t len)
{
    value.toUpperCase();
    if (value.length() > len)
    {
        value = value.substring(0, len);
    }
    return value;
}

static void drawDigit(int x, int y, int digit)
{
    if (digit < 0 || digit > 9)
    {
        return;
    }
    const unsigned char *digits[] = {dd_0, dd_1, dd_2, dd_3, dd_4, dd_5, dd_6, dd_7, dd_8, dd_9};
    dis->drawBitmap(x, y, digits[digit], 16, 25, SCBlack);
}

static void drawSmallDigit(int x, int y, int digit)
{
    if (digit < 0 || digit > 9)
    {
        return;
    }
    const unsigned char *smallDigits[] = {num_0, num_1, num_2, num_3, num_4, num_5, num_6, num_7, num_8, num_9};
    dis->drawBitmap(x, y, smallDigits[digit], 3, 5, SCBlack);
}

static void drawLargeDigit(int x, int y, int digit)
{
    if (digit < 0 || digit > 9)
    {
        return;
    }
    const unsigned char *largeDigits[] = {fd_0, fd_1, fd_2, fd_3, fd_4, fd_5, fd_6, fd_7, fd_8, fd_9};
    dis->drawBitmap(x, y, largeDigits[digit], 33, 53, SCBlack);
}

static void clearTimeArea()
{
    dis->fillRect(0, 0, 200, 70, SCWhite);
}

static void drawAmPm(int hour)
{
#if WATCHFACE_12H
    dis->fillRect(7, 60, 25, 9, SCWhite);
    dis->drawBitmap(7, 60, hour >= 12 ? pm : am, 25, 9, SCBlack);
#else
    (void)hour;
#endif
}

static void showTimeFull()
{
#if LP_CORE
    screenTimeChanged = true;
#endif
    clearTimeArea();

    int hour = timeRTCLocal.Hour;
    drawAmPm(hour);

    if (!use24HourClock())
    {
        if (hour == 0)
        {
            hour = 12;
        }
        else if (hour > 12)
        {
            hour -= 12;
        }
    }

    int minute = timeRTCLocal.Minute;
    drawLargeDigit(11, 5, hour >= 10 ? hour / 10 : 0);
    drawLargeDigit(55, 5, hour % 10);
    drawLargeDigit(111, 5, minute / 10);
    drawLargeDigit(155, 5, minute % 10);
}

static void drawTimeBeforeApply()
{
    showTimeFull();
}

static void drawDate()
{
    dis->fillRect(0, 74, 101, 82, SCWhite);

    dis->setTextColor(SCBlack);
    dis->setTextSize(1);
    dis->setFont(&Seven_Segment10pt7b);

    int16_t x1, y1;
    uint16_t textWidth, textHeight;

    String dayOfWeekName = shortUpper(getLocalizedDayByIndex(timeRTCLocal.Wday, 0), 3);
    dis->getTextBounds(dayOfWeekName, 5, 85, &x1, &y1, &textWidth, &textHeight);
    dis->setCursor(76 - textWidth, 86);
    dis->println(dayOfWeekName);

    String monthName = shortUpper(getLocalizedMonthName(timeRTCLocal.Month), 3);
    dis->getTextBounds(monthName, 60, 110, &x1, &y1, &textWidth, &textHeight);
    dis->setCursor(79 - textWidth, 110);
    dis->println(monthName);

    int day = timeRTCLocal.Day;
    int year = calendarYear(timeRTCLocal);

    drawDigit(8, 95, day / 10);
    drawDigit(29, 95, day % 10);
    drawDigit(8, 129, year / 1000);
    drawDigit(29, 129, (year / 100) % 10);
    drawDigit(50, 129, (year / 10) % 10);
    drawDigit(71, 129, year % 10);
}

static void drawMoon()
{
    dis->fillRect(131, 74, 61, 61, SCWhite);

    moonData_t moon = moonP.getPhase(calendarYear(timeRTCLocal), timeRTCLocal.Month, timeRTCLocal.Day, timeRTCLocal.Hour + 0.1);

    const unsigned char *waxingBitmaps[] = {luna1, luna12, luna11, luna10, luna9, luna8, luna7};
    const unsigned char *waningBitmaps[] = {luna1, luna2, luna3, luna4, luna5, luna6, luna7};
    const unsigned char **bitmaps = (moon.angle <= 180) ? waxingBitmaps : waningBitmaps;

    int index;
    if (moon.percentLit < 0.1)
        index = 0;
    else if (moon.percentLit < 0.25)
        index = 1;
    else if (moon.percentLit < 0.4)
        index = 2;
    else if (moon.percentLit < 0.6)
        index = 3;
    else if (moon.percentLit < 0.75)
        index = 4;
    else if (moon.percentLit < 0.9)
        index = 5;
    else
        index = 6;

    dis->drawBitmap(131, 74, bitmaps[index], 61, 61, SCBlack);
}

static void drawSun()
{
    dis->fillRect(STARFIELD_SUN_X, STARFIELD_SUN_TOP_Y, 3, STARFIELD_SUN_BOTTOM_Y - STARFIELD_SUN_TOP_Y + 5, SCWhite);
    dis->fillRect(116, 62, 20, 10, SCWhite);
    dis->fillRect(116, 132, 20, 10, SCWhite);

    Dusk2Dawn location(String(WEATHER_LATIT).toFloat(), String(WEATHER_LONGTIT).toFloat(), GSR_STARFIELD_TIMEZONE_THING);
    int sunrise = location.sunrise(calendarYear(timeRTCLocal), timeRTCLocal.Month, timeRTCLocal.Day, false);
    int sunset = location.sunset(calendarYear(timeRTCLocal), timeRTCLocal.Month, timeRTCLocal.Day, false);

    long currentTimeInMinutes = timeRTCLocal.Hour * 60 + timeRTCLocal.Minute;
    int sunPosition = 0;
    if (sunset > sunrise)
    {
        sunPosition = (currentTimeInMinutes - sunrise) * 60 / (sunset - sunrise);
    }
    if (currentTimeInMinutes > sunset)
    {
        sunPosition = 60;
    }
    else if (currentTimeInMinutes < sunrise)
    {
        sunPosition = 0;
    }
    sunPosition = constrain(sunPosition, 0, 60);
    dis->drawBitmap(STARFIELD_SUN_X, STARFIELD_SUN_BOTTOM_Y - sunPosition, arr, 3, 5, SCBlack);

    int sunriseHour = sunrise / 60;
    int sunriseMinute = sunrise % 60;
    int sunsetHour = sunset / 60;
    int sunsetMinute = sunset % 60;

    if (!use24HourClock())
    {
        if (sunriseHour == 0)
            sunriseHour = 12;
        else if (sunriseHour > 12)
            sunriseHour -= 12;

        if (sunsetHour == 0)
            sunsetHour = 12;
        else if (sunsetHour > 12)
            sunsetHour -= 12;
    }

    drawSmallDigit(116, 67, sunsetHour / 10);
    drawSmallDigit(120, 67, sunsetHour % 10);
    drawSmallDigit(128, 67, sunsetMinute / 10);
    drawSmallDigit(132, 67, sunsetMinute % 10);
    drawSmallDigit(116, 137, sunriseHour / 10);
    drawSmallDigit(120, 137, sunriseHour % 10);
    drawSmallDigit(128, 137, sunriseMinute / 10);
    drawSmallDigit(132, 137, sunriseMinute % 10);
}

static void drawTimeAfterApply(bool forceDraw)
{
    if (forceDraw || rM.wFTime.Day != timeRTCLocal.Day || rM.wFTime.Month != timeRTCLocal.Month || rM.wFTime.Year != timeRTCLocal.Year)
    {
        drawDate();
    }

    if (forceDraw || lastMoonDay != timeRTCLocal.Day)
    {
        lastMoonDay = timeRTCLocal.Day;
        drawMoon();
    }

    if (forceDraw || lastSunHour != timeRTCLocal.Hour || lastSunMinute != timeRTCLocal.Minute)
    {
        lastSunHour = timeRTCLocal.Hour;
        lastSunMinute = timeRTCLocal.Minute;
        drawSun();
    }
}

static void initWatchface()
{
    dis->fillScreen(SCWhite);
    dis->drawBitmap(0, 0, field, 200, 200, SCBlack);
    // The native module strip replaces Starfield's original steps/Wi-Fi/battery row.
    dis->fillRect(STARFIELD_MODULE_X, STARFIELD_MODULE_Y, STARFIELD_MODULE_W, STARFIELD_MODULE_H, SCWhite);
    readRTC();
    lastMoonDay = 255;
    lastSunHour = 255;
    lastSunMinute = 255;
}

static void drawBattery()
{
    // Battery is intentionally handled by the native module strip instead of the GSR art.
}

static void manageInput(buttonState bt)
{
    switch (bt)
    {
    case Menu:
        generalSwitch(mainMenu);
        break;
    case Up:
        wfModuleSwitch(Right);
        drawModuleCount();
        break;
    case Down:
        wfModuleSwitch(Left);
        drawModuleCount();
        break;
    case LongUp:
    case LongDown:
        wfModulesManage(bt);
        break;
#if LONG_BACK_FULL_REFRESH
    case LongBack:
        debugLog("Because of LONG_BACK_FULL_REFRESH in watchface, I shall now refresh the screen fully");
        updateDisplay(FULL_UPDATE);
        break;
#endif
    default:
        break;
    }
}

const watchfaceDefOne nativeStarfieldDef = {
    .drawTimeBeforeApply = drawTimeBeforeApply,
    .drawTimeAfterApply = drawTimeAfterApply,
    .drawDay = drawDate,
    .drawMonth = drawDate,
    .showTimeFull = showTimeFull,
    .initWatchface = initWatchface,
    .drawBattery = drawBattery,
    .manageInput = manageInput,

    .watchfaceModules = true,
    .watchfaceModSquare = {.size{.w = STARFIELD_MODULE_W, .h = STARFIELD_MODULE_H}, .cord{.x = STARFIELD_MODULE_X, .y = STARFIELD_MODULE_Y}},
    .someDrawingSquare = {.size{.w = 200, .h = 139}, .cord{.x = 0, .y = 61}},
    .isModuleEngaged = []()
    { return false; },
    .lpCoreScreenPrepareCustom = NULL,
};

#endif
