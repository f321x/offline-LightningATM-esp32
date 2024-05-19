#pragma once

#include <Arduino.h>
#include "Bitcoin.h"
#include <Hash.h>
#include <stdexcept>

////////////////////////////////////////////
///////////////LNURL STUFF//////////////////
////USING STEPAN SNIGREVS GREAT CRYTPO//////
////////////THANK YOU STEPAN////////////////
////////////////////////////////////////////

class LnurlPoS {
public:
    LnurlPoS(const String& lnurl_device_string, const bool debug_mode);
    ~LnurlPoS();
    LnurlPoS(const LnurlPoS& other);

    char* makeLNURL(int amount_in_cents);
    String getCurrency() const;

private:
    uint8_t* _secretATM;
    String    _baseURL;
    const bool      _debugMode;
    size_t          _secretLength;
    String          _currencyATM;

    LnurlPoS& operator=(const LnurlPoS& other);
    String _getValue(const String& data, char separator, int index);
    int _xor_encrypt(uint8_t* output, size_t outlen, uint8_t* key, size_t keylen, uint8_t* nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents);
    void _to_upper(char* arr);
};