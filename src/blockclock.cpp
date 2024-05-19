#include "blockclock.hpp"

BlockClock::BlockClock(const std::string& currency, const std::string& wifi_ssid, const std::string& wifi_password, const bool debug_mode)
    : _currency(currency), _wifi_ssid(wifi_ssid.c_str()), _wifi_password(wifi_password.c_str()), _debug_mode(debug_mode) {
    if (currency.empty() || std::find(_supported_currencies.begin(), _supported_currencies.end(), currency) == _supported_currencies.end()) {
        throw std::invalid_argument("Invalid currency for blockclock");
    }
    if (wifi_ssid.empty() || wifi_password.empty() || wifi_ssid == "your_wifi_ssid" || wifi_password == "your_password") {
        throw std::invalid_argument("Invalid wifi credentials for blockclock");
    }
    if (!_connect_wifi()) {
        if (_debug_mode) {
            Serial.println("Failed to connect to WiFi");
        }
        throw std::runtime_error("Failed to connect to WiFi");
    }
    _client.setInsecure();
}

BlockClock::~BlockClock() {
}

BlockClock::BlockClock(const BlockClock& other)
    : _currency(other._currency), _wifi_ssid(other._wifi_ssid), _wifi_password(other._wifi_password), _debug_mode(other._debug_mode) { }

bool BlockClock::_connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifi_ssid, _wifi_password);
    unsigned long start = millis();

    if (_debug_mode) {
        Serial.print("Connecting to: ");
        Serial.println(_wifi_ssid);
    }
    while (WiFi.status() != WL_CONNECTED && millis() - start < _wifi_timeout) {
        delay(500);
        if (_debug_mode)
            Serial.print(".");
    }
    _last_wifi_connection_timestamp = millis();
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    return false;
}

std::string BlockClock::_web_request(const std::string& url) {
    std::string response = "";
    HTTPClient http;

    if (WiFi.status() != WL_CONNECTED) {
        if (_debug_mode) {
            Serial.println("WiFi not connected");
        }
        WiFi.disconnect();
        if (!_connect_wifi())
            return "";
    }
    http.begin(_client, url.c_str());
    int httpCode = http.GET();
    if (httpCode == 200) {
        response = http.getString().c_str();
        if (_debug_mode) {
            Serial.println(httpCode);
            Serial.println(response.c_str());
        }
    }
    else {
        if (_debug_mode)
            Serial.println(http.errorToString(httpCode).c_str());
    }
    http.end();
    return response;
}

uint BlockClock::getBlockHeight() {
    std::string response = _web_request("https://mempool.space/api/blocks/tip/height");
    if (_debug_mode) {
        Serial.print("Block height: ");
        Serial.println(response.c_str());
    }
    if (response.empty()) {
        if (_debug_mode) {
            Serial.println("Failed to get block height");
        }
        return 0;
    }
    return (uint)atoi(response.c_str());
}

uint BlockClock::getExchangeRate() {
    JsonDocument                exchange_rates;
    uint                        sat_per_cuckbuck;

    std::string response = _web_request("https://mempool.space/api/v1/prices");
    DeserializationError error = deserializeJson(exchange_rates, response);
    if (response.empty() || error) {
        if (_debug_mode) {
            Serial.println("Failed to get exchange rate");
        }
        return 0;
    }
    sat_per_cuckbuck = 100000000 / ((uint)exchange_rates[_currency.c_str()]);
    return sat_per_cuckbuck;
}