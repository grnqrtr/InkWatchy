#include "blockwatch.h"

#if WATCHFACE_BLOCKWATCH
#include "rtcMem.h"
#include "moscowBmp.h"
// bitMod.h (and getBitcoinSnapshot) is pulled in through defines.h when BITCOIN_MODULE is set.

// The dot goes hollow as soon as a sync cycle fails, or once the data is older
// than this (covers the case where no sync is even attempted).
#define BLOCKWATCH_STALE_SECONDS 600
// Target block interval. The bar fills one box per minute for the first 10
// minutes, then hollows their centres over the following 10 (blocks average
// ~10 minutes, so this shows "running late" without claiming a new block).
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

// Computes the block height to show (always the last known one, never a future
// block) plus the progress since it was seen: `solid` boxes are filled black
// (one per minute for the first 10) and `hollow` of those have had their centre
// cleared to white (one per minute for the next 10). Both cap at 10.
static void bwBlockProgress(int realHeight, int *solid, int *hollow, int *displayHeight)
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

    // One box per minute; a new block (a changed height) resets the anchor above.
    uint64_t elapsed = now - rM.blockwatch.heightEpochUnix;
    int boxes = (int)((elapsed * BLOCKWATCH_SQUARES) / BLOCKWATCH_BLOCK_SECONDS);

    int s = boxes;
    if (s > BLOCKWATCH_SQUARES)
    {
        s = BLOCKWATCH_SQUARES;
    }
    int h = boxes - BLOCKWATCH_SQUARES;
    if (h < 0)
    {
        h = 0;
    }
    else if (h > BLOCKWATCH_SQUARES)
    {
        h = BLOCKWATCH_SQUARES;
    }

    *displayHeight = height;
    *solid = s;
    *hollow = h;
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
    int solid = 0;
    int hollow = 0;
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
        bwBlockProgress(realHeight, &solid, &hollow, &dispHeight);
        value = String(dispHeight);
    }

    setTextSize(1);
    const GFXfont *valueFont = bwPickFont(value, fontList, fontCount, 190);
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
        int x = gap / 2;                  // 2
        int y = 174;
        int inset = 5; // white centre of a "hollowed" box
        for (int i = 0; i < squares; i++)
        {
            if (i < hollow)
            {
                // Filled black, but the centre has been cleared to white.
                dis->fillRect(x, y, side, side, SCBlack);
                dis->fillRect(x + inset, y + inset, side - 2 * inset, side - 2 * inset, SCWhite);
            }
            else if (i < solid)
            {
                dis->fillRect(x, y, side, side, SCBlack);
            }
            else
            {
                dis->drawRect(x, y, side, side, SCBlack);
            }
            x += gap + side;
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
