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
    BlockClock();
    ~BlockClock();
    BlockClock(const BlockClock& other);

    void init(const String& currency, const String& wifi_ssid, const String& wifi_password, const bool debug_mode);
    uint getBlockHeight();
    uint getExchangeRate();

private:
    BlockClock& operator=(const BlockClock& other);

    String _currency;
    String _wifi_ssid;
    String _wifi_password;
    bool _debug_mode;
    bool _blockclock_deactivated;
    WiFiClientSecure _client;
    std::vector<String> _supported_currencies = { "USD", "EUR", "GBP", "CAD", "CHF", "AUD", "JPY" };
    unsigned long   _last_wifi_connection_timestamp;
    const unsigned long _wifi_timeout = 15000;

    std::string _web_request(const String& url);
    bool _connect_wifi();
};