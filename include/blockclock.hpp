#pragma once

#include <string>
#include <vector>
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoHttpClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <stdexcept>


class BlockClock {
public:
    BlockClock(const std::string& currency, const std::string& wifi_ssid, const std::string& wifi_password, const bool debug_mode = false);
    ~BlockClock();
    BlockClock(const BlockClock& other);

    uint getBlockHeight();
    uint getExchangeRate();

private:
    BlockClock& operator=(const BlockClock& other);

    const std::string _currency;
    const char* _wifi_ssid;
    const char* _wifi_password;
    const bool _debug_mode;
    bool _blockclock_deactivated;
    WiFiClientSecure _client;
    std::vector<std::string> _supported_currencies = { "USD", "EUR", "GBP", "CAD", "CHF", "AUD", "JPY" };
    unsigned long   _last_wifi_connection_timestamp;
    const unsigned long _wifi_timeout = 15000;

    std::string _web_request(const std::string& url);
    bool _connect_wifi();
};