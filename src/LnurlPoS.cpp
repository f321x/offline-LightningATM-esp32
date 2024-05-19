#include "LnurlPoS.hpp"

LnurlPoS::LnurlPoS() : _initialized(false) { }

LnurlPoS::~LnurlPoS() { }

LnurlPoS::LnurlPoS(const LnurlPoS& other) {
    _secretATM = other._secretATM;
    _baseURL = other._baseURL;
    _currencyATM = other._currencyATM;
    _debugMode = other._debugMode;
    _secretLength = other._secretLength;
    _initialized = other._initialized;
}

LnurlPoS& LnurlPoS::operator=(const LnurlPoS& other) {
    if (this == &other)
        return *this;
    _secretATM = other._secretATM;
    _baseURL = other._baseURL;
    _currencyATM = other._currencyATM;
    _debugMode = other._debugMode;
    _secretLength = other._secretLength;
    _initialized = other._initialized;
    return *this;
}

void    LnurlPoS::init(const String& lnurl_device_string, const bool debug_mode) {
    _debugMode = debug_mode;
    if (_debugMode)
        Serial.println("LnurlPoS created");
    if (lnurl_device_string == "https://legend.lnbits.com/lnurldevice/api/v1/lnurl/idexample,keyexample,EUR" && _debugMode) {
        Serial.println("LnurlPoS: WARNING: using default lnurl_device_string");
    }
    if (lnurl_device_string.length() == 0) {
        throw std::invalid_argument("LnurlPoS: lnurl_device_string must be non-empty");
    }
    _baseURL = _getValue(lnurl_device_string, ',', 0);
    String secretATMbuf = _getValue(lnurl_device_string, ',', 1);
    _currencyATM = _getValue(lnurl_device_string, ',', 2);

    _secretLength = secretATMbuf.length();
    _secretATM = (uint8_t*)secretATMbuf.c_str();
    if (_secretATM == NULL || _secretLength == 0 || _baseURL.length() == 0) {
        throw std::invalid_argument("LnurlPoS: secretATM and baseURL must be non-empty");
    }
    _initialized = true;
}

// Function to seperate the LNURLDevice string in key, url and currency
String LnurlPoS::_getValue(const String& data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    const int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data.charAt(i) == separator || i == maxIndex)
        {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i + 1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String LnurlPoS::getCurrency() const
{
    if (!_initialized)
    {
        throw std::invalid_argument("LnurlPoS: not initialized");
    }
    return _currencyATM;
}

int LnurlPoS::_xor_encrypt(uint8_t* output, size_t outlen, uint8_t* key, size_t keylen, uint8_t* nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents)
{
    // check we have space for all the data:
    // <variant_byte><len|nonce><len|payload:{pin}{amount}><hmac>
    if (outlen < 2 + nonce_len + 1 + lenVarInt(pin) + 1 + lenVarInt(amount_in_cents) + 8)
    {
        return 0;
    }

    int cur = 0;
    output[cur] = 1; // variant: XOR encryption
    cur++;

    // nonce_len | nonce
    output[cur] = nonce_len;
    cur++;
    memcpy(output + cur, nonce, nonce_len);
    cur += nonce_len;

    // payload, unxored first - <pin><currency byte><amount>
    int payload_len = lenVarInt(pin) + 1 + lenVarInt(amount_in_cents);
    output[cur] = (uint8_t)payload_len;
    cur++;
    uint8_t* payload = output + cur;                                 // pointer to the start of the payload
    cur += writeVarInt(pin, output + cur, outlen - cur);             // pin code
    cur += writeVarInt(amount_in_cents, output + cur, outlen - cur); // amount
    cur++;

    // xor it with round key
    uint8_t hmacresult[32];
    SHA256 h;
    h.beginHMAC(key, keylen);
    h.write((uint8_t*)"Round secret:", 13);
    h.write(nonce, nonce_len);
    h.endHMAC(hmacresult);
    for (int i = 0; i < payload_len; i++)
    {
        payload[i] = payload[i] ^ hmacresult[i];
    }

    // add hmac to authenticate
    h.beginHMAC(key, keylen);
    h.write((uint8_t*)"Data:", 5);
    h.write(output, cur);
    h.endHMAC(hmacresult);
    memcpy(output + cur, hmacresult, 8);
    cur += 8;

    // return number of bytes written to the output
    return cur;
}

void LnurlPoS::_to_upper(char* arr)
{
    for (size_t i = 0; i < strlen(arr); i++)
    {
        if (arr[i] >= 'a' && arr[i] <= 'z')
        {
            arr[i] = arr[i] - 'a' + 'A';
        }
    }
}

String LnurlPoS::makeLNURL(int total)
{
    if (!_initialized)
    {
        throw std::invalid_argument("LnurlPoS: not initialized");
    }
    int randomPin = random(1000, 9999);
    byte nonce[8];
    for (int i = 0; i < 8; i++)
    {
        nonce[i] = random(256);
    }
    byte payload[51]; // 51 bytes is max one can get with xor-encryption
    size_t payload_len = _xor_encrypt(payload, sizeof(payload), _secretATM, _secretLength, nonce, sizeof(nonce), randomPin, total);
    String preparedURL = _baseURL + "?atm=1&p=";
    preparedURL += toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);
    if (_debugMode)
        Serial.println(preparedURL);
    char Buf[200];
    preparedURL.toCharArray(Buf, 200);
    char* url = Buf;
    byte* data = (byte*)calloc(strlen(url) * 2, sizeof(byte));
    if (!data)
        return (String(""));
    size_t len = 0;
    int res = convert_bits(data, &len, 5, (byte*)url, strlen(url), 8, 1);
    char* charLnurl = (char*)calloc(strlen(url) * 2, sizeof(byte));
    if (!charLnurl)
    {
        free(data);
        return (String(""));
    }
    bech32_encode(charLnurl, "lnurl", data, len);
    _to_upper(charLnurl);
    free(data);
    String lnurl(charLnurl);
    free(charLnurl);
    return (lnurl);
}

