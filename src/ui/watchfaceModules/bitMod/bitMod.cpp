#include "bitMod.h"
#include "rtcMem.h"

#if BITCOIN_MODULE

#include <HTTPClient.h>

#if SYNC_PROXY
// Plaintext proxy on the phone hotspot; serves the same paths (see config).
#define MEMPOOL_BASEURL SYNC_PROXY_BASE
#define BINANCE_BASEURL SYNC_PROXY_BASE "/api/v3/klines?symbol=BTCUSDT&interval=1m&limit=1&startTime="
#else
#define MEMPOOL_BASEURL "https://mempool.space/api"
#define BINANCE_BASEURL "https://api.binance.com/api/v3/klines?symbol=BTCUSDT&interval=1m&limit=1&startTime="
#endif

bitcoinData btcData = {0};
bool btcDataLoaded = false;
std::mutex btcDataMutex;

uint getBitcoinSyncAgeMinutes(long lastSyncUnix)
{
    uint64_t currentUnix = getUnixTime(timeRTCLocal);
    if (lastSyncUnix <= 0 || currentUnix <= static_cast<uint64_t>(lastSyncUnix))
    {
        return 0;
    }
    return (currentUnix - static_cast<uint64_t>(lastSyncUnix)) / 60;
}

bool saveBitcoinData(bitcoinData data)
{
    size_t dataSize = sizeof(bitcoinData);
    if (fsSetBlob(CONF_BITCOIN, (uint8_t *)&data, dataSize) == false)
    {
        debugLog("Failed to save bitcoin data; keeping previous snapshot");
        return false;
    }

    std::lock_guard<std::mutex> lock(btcDataMutex);
    btcData = data;
    btcDataLoaded = true;
    rM.isBtcDataAvail = true;
    return true;
}

bool loadBitcoinData()
{
    size_t dataSize = sizeof(bitcoinData);
    bufSize serializedData = fsGetBlob(CONF_BITCOIN);
    debugLog("bitcoin serializedData.size: " + String(serializedData.size));

    if (serializedData.size != dataSize)
    {
        debugLog("Bitcoin data size is wrong");
        if (serializedData.buf != NULL)
        {
            free(serializedData.buf);
        }
        std::lock_guard<std::mutex> lock(btcDataMutex);
        if (btcDataLoaded == false)
        {
            rM.isBtcDataAvail = false;
        }
        return false;
    }

    bitcoinData loadedData;
    memcpy(&loadedData, serializedData.buf, dataSize);
    free(serializedData.buf);

    std::lock_guard<std::mutex> lock(btcDataMutex);
    btcData = loadedData;
    btcDataLoaded = true;
    rM.isBtcDataAvail = true;
    return true;
}

void wfBitcheckShow(bool *showBool, bool *redrawBool)
{
#if MODULE_PERSISTENT
    *showBool = true;
#else
    if (rM.isBtcDataNew == true && rM.isBtcDataAvail == true)
    {
        *showBool = true;
    }
#endif
    long lastSyncUnix;
    {
        std::lock_guard<std::mutex> lock(btcDataMutex);
        lastSyncUnix = btcData.btcLastSyncUnix;
    }
    if (rM.isBtcDataAvail == true && (rM.isBtcDataNew == true || getBitcoinSyncAgeMinutes(lastSyncUnix) != rM.btcLastUpdate))
    {
        debugLog("btc redraw bool is true!");
        *redrawBool = true;
    }
}

#define SYNC_INFO_OFFSET 2
void wfBitrequestShow(buttonState button, bool *showBool)
{
#if MODULE_PERSISTENT == false
    if (button == Menu)
    {
        *showBool = false;
        return;
    }
#endif
    if (button == Down)
    {
        rM.smallBtcData = !rM.smallBtcData;
        clearModuleArea();
    }
    debugLog("Launched");
    bool needsLoad;
    {
        std::lock_guard<std::mutex> lock(btcDataMutex);
        needsLoad = btcDataLoaded == false;
    }
    if (needsLoad == true)
    {
        loadBitcoinData();
    }

    bitcoinData displayData = {0};
    bool dataAvailable;
    {
        std::lock_guard<std::mutex> lock(btcDataMutex);
        displayData = btcData;
        dataAvailable = btcDataLoaded == true && rM.isBtcDataAvail == true;
    }
    squareInfo modSq = getWatchModuleSquare();
    if (rM.smallBtcData == true)
    {

        writeImageN(modSq.cord.x, modSq.cord.y, getImg("bitcoin"));
        writeImageN(modSq.cord.x, modSq.cord.y + modSq.size.h - getImgWidth("pickaxe"), getImg("pickaxe"));
        if (dataAvailable == true)
        {
            dis->setCursor(modSq.cord.x + getImgWidth("pickaxe"), modSq.cord.y + modSq.size.h - 1);
            setFont(getFont("dogicapixel4"));
            setTextSize(1);
            dis->print(displayData.height);
        }
        setFont(getFont("dogicapixel4"));
        setTextSize(1);
        dis->setCursor(modSq.cord.x + modSq.size.w - 60, modSq.cord.y + 7 - 1); // font is 7 pixels
        dis->print("Last sync:");

        setFont(getFont("dogicapixel4"));
        dis->setCursor(modSq.cord.x + modSq.size.w, modSq.cord.y + 7);
        String lastSync = "Never";
        if (dataAvailable == true)
        {
            uint diff = getBitcoinSyncAgeMinutes(displayData.btcLastSyncUnix);
            rM.btcLastUpdate = diff;
            debugLog("diff: " + String(diff));
            lastSync = String(diff) + "m ago";
        }
        uint16_t h;
        uint16_t w;
        getTextBounds(lastSync, NULL, NULL, &w, &h);
        dis->setCursor(modSq.cord.x + modSq.size.w - w - SYNC_INFO_OFFSET, modSq.cord.y + 7 + h + SYNC_INFO_OFFSET);
        dis->print(lastSync);

        if (dataAvailable == true && (displayData.change1h != 0.0 || displayData.change24h != 0.0 || displayData.change7d != 0.0 || displayData.change30d != 0.0 || displayData.price != 0.0))
        {
            dis->setCursor(modSq.cord.x + getImgWidth("bitcoin"), modSq.cord.y + getImgHeight("bitcoin") - 2);
            setFont(getFont("dogicapixel4"));
            setTextSize(1);
            dis->print(":" + String(displayData.price) + "$");
            dis->setCursor(modSq.cord.x, modSq.cord.y + getImgHeight("bitcoin") * 2);
            dis->print("1h:" + String(displayData.change1h) + "% 24h:" + String(displayData.change24h) + "%");
            dis->setCursor(modSq.cord.x, modSq.cord.y + getImgHeight("bitcoin") * 3);
            dis->print("7d:" + String(displayData.change7d) + "% 30d:" + String(displayData.change30d) + "%");
        }
        else
        {
            debugLog("Not printing btc data?");
        }
    }
    else
    {
        // Sync time
        setFont(getFont("dogicapixel4"));
        setTextSize(1);
        dis->setCursor(modSq.cord.x + modSq.size.w - 60, modSq.cord.y + 7 - 1); // font is 7 pixels
        dis->print("Last sync:");

        setFont(getFont("dogicapixel4"));
        dis->setCursor(modSq.cord.x + modSq.size.w, modSq.cord.y + 7);
        String lastSync = "Never";
        if (dataAvailable == true)
        {
            uint diff = getBitcoinSyncAgeMinutes(displayData.btcLastSyncUnix);
            rM.btcLastUpdate = diff;
            debugLog("diff: " + String(diff));
            lastSync = String(diff) + "m ago";
        }
        uint16_t h;
        uint16_t w;
        getTextBounds(lastSync, NULL, NULL, &w, &h);
        dis->setCursor(modSq.cord.x + modSq.size.w - w - SYNC_INFO_OFFSET, modSq.cord.y + 7 + h + SYNC_INFO_OFFSET);
        dis->print(lastSync);

        // Bitclock
        dis->setCursor(modSq.cord.x, modSq.cord.y + modSq.size.h - 1);
        setFont(getFont("smileandwave20"));
        setTextSize(1);
        if (dataAvailable == true)
        {
            dis->print(displayData.height);
        }
    }

    rM.isBtcDataNew = false;
    dUChange = true;
}

int getBitcoinPriceForUnix(uint64_t unixTime)
{
    HTTPClient http;
    // Generous limits: modem-sleep power saving slows handshakes and the
    // default 5s timeout aborts slow-but-working links.
    http.setTimeout(25000);
    http.setConnectTimeout(15000);
    http.addHeader("Content-Type", "application/json");
    // http.addHeader("User-Agent", "YourUserAgent");
    String binanceUrl = String(BINANCE_BASEURL) + String((unixTime - 1800) * 1000); // - 30m as we can make it for this exact second
    debugLog("Binance URL: " + binanceUrl);
    bool beginErr = http.begin(binanceUrl);
    if (beginErr == false)
    {
        debugLog("beginErr is 0");
        return 0;
    }
    int httpCode = http.GET();
    debugLog("httpCode is: " + String(httpCode));
    if (httpCode == HTTP_CODE_OK)
    {
        String output = http.getString();
        http.end();
        output = output.substring(1, output.length() - 1);
        debugLog("Received from binance: " + output);

        // No ArduinoJson here on purpose: with the heap this fragmented
        // during syncs (largest free block ~768B observed), parsing even a
        // 170B payload fails with NoMemory. Index parsing needs no heap.
        // Kline format: [openTime,"open","high","low","close",...]
        int c1 = output.indexOf(',');
        int c2 = output.indexOf(',', c1 + 1);
        int c3 = output.indexOf(',', c2 + 1);
        int c4 = output.indexOf(',', c3 + 1);
        float two = 0, thri = 0;
        if (c1 > 0 && c2 > c1 && c3 > c2 && c4 > c3)
        {
            two = output.substring(c2 + 2, c3 - 1).toFloat();
            thri = output.substring(c3 + 2, c4 - 1).toFloat();
        }
        if (two == 0 || thri == 0)
        {
            debugLog("Failed to parse kline");
            return 0;
        }
        debugLog("two is: " + String(two));
        int average = int(round((two + thri) / 2));
        debugLog("Final average is: " + String(average));
        return average;
    }
    http.end();
    return 0;
}

float roundToDecimal(float value, int decimalPlaces) {
    float scale = pow(10.0, decimalPlaces);
    return round(value * scale) / scale;
}

// One small request for the current tip height. Returns 0 on any failure.
static int fetchBitcoinHeight()
{
    HTTPClient http;
    // Same generous limits as the price request.
    http.setTimeout(25000);
    http.setConnectTimeout(15000);
    http.begin(String(MEMPOOL_BASEURL) + "/blocks/tip/height");
    int httpCode = http.GET();
    debugLog("Height http code: " + String(httpCode));
    int height = 0;
    if (httpCode == HTTP_CODE_OK)
    {
        String body = http.getString();
        http.end();
        height = body.toInt();
    }
    else
    {
        http.end();
    }
    debugLog("Got bitcoin height: " + String(height));
    return height;
}

// Spot price plus the reference prices behind the change percentages. Fills
// `out.price`/`out.change*` and returns false if anything is missing.
static bool fetchBitcoinPriceData(bitcoinData &out)
{
    uint64_t unixTimeNow = getUnixTime(timeRTCUTC0);
    int price = getBitcoinPriceForUnix(unixTimeNow);
    int price1h = getBitcoinPriceForUnix(unixTimeNow - 3600);
    int price24h = getBitcoinPriceForUnix(unixTimeNow - 86400);
    int price7d = getBitcoinPriceForUnix(unixTimeNow - 604800);
    int price30d = getBitcoinPriceForUnix(unixTimeNow - 2592000);

    if (price == 0 || price1h == 0 || price24h == 0 || price7d == 0 || price30d == 0)
    {
        return false;
    }

    out.price = price;
    out.change1h = roundToDecimal((float(price - price1h) / float(price1h)) * 100.0, 1);
    out.change24h = roundToDecimal((float(price - price24h) / float(price24h)) * 100.0, 1);
    out.change7d = roundToDecimal((float(price - price7d) / float(price7d)) * 100.0, 1);
    out.change30d = roundToDecimal((float(price - price30d) / float(price30d)) * 100.0, 1);
    return true;
}

// Height-only sync: one small request, merged into the current snapshot so it
// never clobbers price data (and the price sync never clobbers the height).
// Meant to run far more often than the price sync.
void bitcoinHeightSync(uint8_t tries)
{
    debugLog("Launching bitcoinHeightSync");
    if (tries > BITCOIN_SYNC_TRIES)
    {
        debugLog("Too many height tries, giving up");
        rM.isBtcHeightFailed = true;
        return;
    }

    int height = fetchBitcoinHeight();
    if (height <= 0)
    {
        debugLog("Height retry, next try: " + String(tries + 1));
        // Yield so failing retries can't starve lower-priority tasks
        // (scom serial task, main loop) into apparent silence.
        delayTask(2000);
        bitcoinHeightSync(tries + 1);
        return;
    }

    if (btcDataLoaded == false)
    {
        loadBitcoinData();
    }

    bitcoinData snapshot;
    bool heightChanged;
    {
        std::lock_guard<std::mutex> lock(btcDataMutex);
        heightChanged = btcData.height != height;
        btcData.height = height;
        snapshot = btcData;
    }
    rM.btcLastHeightSyncUnix = getUnixTime(timeRTCLocal);

    // Only write flash when the height actually moved (a block averages ~10
    // min): a 1-minute poll must not wear the filesystem.
    if (heightChanged)
    {
        if (saveBitcoinData(snapshot) == true)
        {
            rM.isBtcDataNew = true;
            rM.isBtcHeightFailed = false;
        }
        else
        {
            rM.isBtcHeightFailed = true;
        }
    }
    else
    {
        rM.isBtcHeightFailed = false;
    }
}

// Price-only sync: spot + reference prices, merged into the current snapshot
// (keeps the height and its timestamp).
void bitcoinPriceSync(uint8_t tries)
{
    debugLog("Launching bitcoinPriceSync");
    if (tries > BITCOIN_SYNC_TRIES)
    {
        debugLog("Too many price tries, giving up");
        rM.isBtcPriceFailed = true;
        return;
    }

    bitcoinData priceData = {0};
    if (fetchBitcoinPriceData(priceData) == false)
    {
        debugLog("Price retry, next try: " + String(tries + 1));
        delayTask(2000);
        bitcoinPriceSync(tries + 1);
        return;
    }

    if (btcDataLoaded == false)
    {
        loadBitcoinData();
    }
    {
        std::lock_guard<std::mutex> lock(btcDataMutex);
        priceData.height = btcData.height;
    }
    priceData.btcLastSyncUnix = getUnixTime(timeRTCLocal);

    // Without this the module never redraws with the new data: the
    // show-check only refreshes on new data or a changed hour-diff.
    if (saveBitcoinData(priceData) == true)
    {
        rM.isBtcDataNew = true;
        rM.isBtcPriceFailed = false;
    }
    else
    {
        rM.isBtcPriceFailed = true;
    }
}

// Full sync (height + price), used by the regular / kind-of-persistent paths.
void bitcoinSyncAttempt(uint8_t tries, bitcoinData &newData)
{
    debugLog("Launching bitcoinSync");
    if (tries > BITCOIN_SYNC_TRIES)
    {
        debugLog("Too many tries, exiting");
        rM.isBtcHeightFailed = true;
        rM.isBtcPriceFailed = true;
        return;
    }

    int height = fetchBitcoinHeight();
    bitcoinData priceData = {0};
    bool priceOk = fetchBitcoinPriceData(priceData);

    if (height <= 0 || priceOk == false)
    {
        debugLog("Running again, next try: " + String(tries + 1));
        delayTask(2000);
        bitcoinSyncAttempt(tries + 1, newData);
        return;
    }

    debugLog("Bitcoin is fine!");
    newData = priceData;
    newData.height = height;
    newData.btcLastSyncUnix = getUnixTime(timeRTCLocal);
    rM.btcLastHeightSyncUnix = newData.btcLastSyncUnix;
    if (saveBitcoinData(newData) == true)
    {
        rM.isBtcDataNew = true;
        rM.isBtcHeightFailed = false;
        rM.isBtcPriceFailed = false;
    }
    else
    {
        rM.isBtcHeightFailed = true;
        rM.isBtcPriceFailed = true;
    }
}

void bitcoinSync(uint8_t tries)
{
    // Keep the current display and persisted data untouched until a complete
    // replacement snapshot has been fetched and saved successfully.
    bitcoinData newData = {0};
    bitcoinSyncAttempt(tries, newData);
}

#endif
