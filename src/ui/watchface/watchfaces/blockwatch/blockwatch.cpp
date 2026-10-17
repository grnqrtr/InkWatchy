#include "blockwatch.h"

#if WATCHFACE_BLOCKWATCH
#include "rtcMem.h"
#include "moscowBmp.h"
// bitMod.h (and getBitcoinSnapshot) is pulled in through defines.h when BITCOIN_MODULE is set.

// The dot goes hollow as soon as a sync cycle fails, or once the data is older
// than this (covers the case where no sync is even attempted).
#define BLOCKWATCH_STALE_SECONDS 600
// Target block interval. The bottom row has one box per minute; after a full
// 10 minutes the row resets and a "bar" is added above it, one more bar for
// each further 10 minutes. So lateness reads as bars*10 + boxes minutes.
#define BLOCKWATCH_BLOCK_SECONDS 600
#define BLOCKWATCH_SQUARES 10

// "Square27" is the original BLOCKWATCH font, installed in the watch filesystem.
// The rest are fallbacks so a too-wide value (e.g. a 7-digit height) still fits.
static const char *const BW_BLOCK_FONTS[] = {"Square27", "Mono33", "shades/RampartOne30", "taychron/Mono30", "JackInput17"};
static const char *const BW_MOSCOW_FONTS[] = {"Square27", "Mono33", "taychron/Mono30", "JackInput17"};

// Sum of the glyph advances, which is exactly what print() will consume.
static uint16_t bwTextWidth(const GFXfont *f, const String &text)
{
    if (f == NULL)
    {
        return 0;
    }
    uint16_t w = 0;
    for (uint16_t i = 0; i < text.length(); i++)
    {
        uint8_t c = (uint8_t)text[i];
        if (c < f->first || c > f->last)
        {
            continue;
        }
        w += f->glyph[c - f->first].xAdvance;
    }
    return w;
}

static const GFXfont *bwPickFont(String text, const char *const *names, uint8_t count, uint16_t maxWidth)
{
    const GFXfont *fallback = NULL;
    for (uint8_t i = 0; i < count; i++)
    {
        const GFXfont *f = getFont(names[i]);
        if (f == NULL)
        {
            continue;
        }
        fallback = f;
        setFont(f);
        setTextSize(1);
        if (bwTextWidth(f, text) <= maxWidth)
        {
            return f;
        }
    }
    return fallback;
}

static void bwReadData(int *height, int *price, bool *available, bool *live)
{
    *height = 0;
    *price = 0;
    *available = false;
    *live = false;
#if BITCOIN_MODULE
    bool avail = false;
    bitcoinData d = getBitcoinSnapshot(&avail);
    *height = d.height;
    *price = d.price;
    *available = avail;
    uint64_t now = getUnixTime(timeRTCLocal);
    // The face shows the height, so freshness tracks the height-only poll.
    long lastHeightSync = rM.btcLastHeightSyncUnix;
    if (avail && lastHeightSync > 0 && now >= (uint64_t)lastHeightSync)
    {
        *live = (now - (uint64_t)lastHeightSync) <= BLOCKWATCH_STALE_SECONDS;
    }
    // A failed height sync marks the data stale immediately, even if still fresh.
    if (rM.isBtcHeightFailed)
    {
        *live = false;
    }
#endif
}

// Computes the last known height (never a future block) plus how long ago it
// was seen. The 10 boxes fill one per minute within the current 10 minutes;
// each completed 10 minutes adds a `bar` above the row, so bars*10 + filled is
// the whole number of minutes since the block.
static void bwBlockProgress(int realHeight, int *filled, int *bars, int *displayHeight)
{
    uint64_t now = getUnixTime(timeRTCLocal);

    if (realHeight > 0 && realHeight != rM.blockwatch.lastHeight)
    {
        rM.blockwatch.lastHeight = realHeight;
        rM.blockwatch.heightEpochUnix = now;
    }
    if (rM.blockwatch.heightEpochUnix == 0 || rM.blockwatch.heightEpochUnix > now)
    {
        rM.blockwatch.heightEpochUnix = now;
    }

    int height = (realHeight > 0) ? realHeight : rM.blockwatch.lastHeight;

    int minute = (int)((now - rM.blockwatch.heightEpochUnix) / 60);

    int f = 0;
    int b = 0;
    if (minute > 0)
    {
        f = ((minute - 1) % BLOCKWATCH_SQUARES) + 1;
        b = (minute - 1) / BLOCKWATCH_SQUARES;
    }
    if (b > BLOCKWATCH_SQUARES)
    {
        b = BLOCKWATCH_SQUARES;
    }

    *displayHeight = height;
    *filled = f;
    *bars = b;
}

// Small dot in the top-right corner showing whether the bitcoin data is fresh.
static void bwDrawLiveDot(bool live)
{
    if (live)
    {
        dis->fillCircle(192, 8, 4, SCBlack);
    }
    else
    {
        dis->drawCircle(192, 8, 4, SCBlack);
    }
}

static void bwDrawFace()
{
    int realHeight = 0;
    int price = 0;
    bool available = false;
    bool live = false;
    bwReadData(&realHeight, &price, &available, &live);
    bool moscow = rM.blockwatch.moscowTime;

    dis->fillRect(0, 0, 200, 200, SCWhite);

    String value;
    int filled = 0;
    int bars = 0;
    const char *const *fontList = BW_BLOCK_FONTS;
    uint8_t fontCount = sizeof(BW_BLOCK_FONTS) / sizeof(BW_BLOCK_FONTS[0]);
    int16_t baseline = 118;

    if (moscow)
    {
        dis->drawBitmap(0, 0, MOSCOW_BMP_DATA, MOSCOW_BMP_WIDTH, MOSCOW_BMP_HEIGHT, SCBlack);
        int t = (price > 0) ? (100000000 / price) : 0;
        String s = String(t);
        while (s.length() < 4)
        {
            s = "0" + s;
        }
        value = s.substring(0, 2) + ":" + s.substring(2, 4);
        fontList = BW_MOSCOW_FONTS;
        fontCount = sizeof(BW_MOSCOW_FONTS) / sizeof(BW_MOSCOW_FONTS[0]);
        baseline = 192;
    }
    else
    {
        int dispHeight = 0;
        bwBlockProgress(realHeight, &filled, &bars, &dispHeight);
        value = String(dispHeight);
    }

    setTextSize(1);
    // 196 leaves Square27's widest 6-digit height (192) fitting, so the font
    // never switches just because the value happens to contain no '1'.
    const GFXfont *valueFont = bwPickFont(value, fontList, fontCount, 196);
    setFont(valueFont);
    dis->setTextWrap(false);
    dis->setTextColor(SCBlack);
    uint16_t vw = bwTextWidth(valueFont, value);
    int vx = (200 - (int)vw) / 2;
    if (vx < 0)
    {
        vx = 0;
    }
    dis->setCursor(vx, baseline);
    dis->print(value);

    if (!moscow)
    {
        int squares = BLOCKWATCH_SQUARES;
        int gap = 4;
        int side = (200 / squares) - gap; // 16
        int x0 = gap / 2;                 // 2
        int boxY = 174;
        int barH = 8;
        int barY = boxY - gap - barH; // a bar sits above each box, with a gap
        for (int i = 0; i < squares; i++)
        {
            int x = x0 + i * (gap + side);
            // One extra bar per completed 10 minutes.
            if (i < bars)
            {
                dis->fillRect(x, barY, side, barH, SCBlack);
            }
            // One box per minute within the current 10.
            if (i < filled)
            {
                dis->fillRect(x, boxY, side, side, SCBlack);
            }
            else
            {
                dis->drawRect(x, boxY, side, side, SCBlack);
            }
        }
    }

    bwDrawLiveDot(live);

    // Battery as a thin bar along the bottom edge.
    dis->fillRect(0, 198, 200, 2, SCWhite);
    dis->fillRect(0, 198, (rM.bat.percentage * 200) / 100, 2, SCBlack);

    dUChange = true;
}

static void bwManageInput(buttonState bt)
{
    switch (bt)
    {
    case Menu:
    {
        generalSwitch(mainMenu);
        break;
    }
    case Up:
    case Down:
    {
        rM.blockwatch.moscowTime = !rM.blockwatch.moscowTime;
        bwDrawFace();
        break;
    }
    default:
    {
        break;
    }
    }
}

const watchfaceDefOne blockwatchDef = {
    .drawTimeBeforeApply = bwDrawFace,
    .drawTimeAfterApply = [](bool forceDraw) {},
    .drawDay = []() {},
    .drawMonth = []() {},
    .showTimeFull = bwDrawFace,
    .initWatchface = []() {},
    .drawBattery = bwDrawFace,
    .manageInput = bwManageInput,

    .watchfaceModules = false,
    .watchfaceModSquare = {.size{.w = 0, .h = 0}, .cord{.x = 0, .y = 0}},
    .someDrawingSquare = {.size{.w = 0, .h = 0}, .cord{.x = 0, .y = 0}},
    .isModuleEngaged = []()
    { return false; },
    .lpCoreScreenPrepareCustom = NULL,
};

#endif
