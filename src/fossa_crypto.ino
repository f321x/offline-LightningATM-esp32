// modified code from https://github.com/lnbits/fossa, thanks lnbits!
#include "fossa_crypto.hpp"

// --- AES-CBC encrypt ---
void encrypt(unsigned char* key, unsigned char* iv, int length, const char* plainText, unsigned char* outputBuffer) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    // AES-256 key is 32 bytes (256 bits)
    mbedtls_aes_setkey_enc(&aes, key, 256);

    // Note: mbedtls_aes_crypt_cbc mutates the IV internally
    unsigned char working_iv[16];
    memcpy(working_iv, iv, sizeof(working_iv));

    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, (size_t)length,
        working_iv, (const unsigned char*)plainText, outputBuffer);

    mbedtls_aes_free(&aes);
}


// --- Derive 32-byte key + 16-byte IV from secret + 8-byte salt (EVP_BytesToKey-like, MD5 rounds) ---
void deriveKeyAndIV(const char* secret, unsigned char* salt, unsigned char* outKeyIV) {
    // outKeyIV must be 48 bytes: first 32 = key, next 16 = IV
    // We generate MD5 blocks until we have 48 bytes.
    unsigned char md5_buf[16];
    size_t have = 0;

    const unsigned char* sec = (const unsigned char*)secret;
    size_t sec_len = strlen(secret);

    mbedtls_md5_context ctx;

    while (have < 48) {
        mbedtls_md5_init(&ctx);
        // D_i = MD5( D_{i-1} (if any) + secret + salt )
        if (have > 0) {
            mbedtls_md5_update(&ctx, md5_buf, 16);
        }
        if (sec_len > 0) {
            mbedtls_md5_update(&ctx, sec, sec_len);
        }
        mbedtls_md5_update(&ctx, salt, 8);
        mbedtls_md5_finish(&ctx, md5_buf);
        mbedtls_md5_free(&ctx);

        size_t to_copy = (48 - have) < 16 ? (48 - have) : 16;
        memcpy(outKeyIV + have, md5_buf, to_copy);
        have += to_copy;
    }
}

// --- Build LNURL (prints URL and LNURL, sets global qrData) ---
String makeLNURLFossa(String secretATM, float total, String baseURLATM) {
    // 8-byte salt
    unsigned char salt[8];
    for (int i = 0; i < 8; i++) {
        salt[i] = (unsigned char)random(0, 255);
    }

    // Derive key+iv (48 bytes)
    unsigned char keyIV[32 + 16] = { 0 };
    deriveKeyAndIV(secretATM.c_str(), salt, keyIV);

    unsigned char key[32];
    unsigned char iv[16];
    memcpy(key, keyIV, 32);
    memcpy(iv, keyIV + 32, 16);

    // Random 4-digit PIN
    int randomPin = (int)random(1000, 9999);

    // Payload "PIN:total" + PKCS7 padding to 16 bytes
    String payload = String(randomPin) + ":" + String(total);
    size_t payload_len = payload.length();
    int padding = 16 - (payload_len % 16);
    if (padding == 16) padding = 16;
    payload_len += padding;

    // Create padded copy
    String padded = payload;
    for (int i = 0; i < padding; i++) {
        padded += (char)padding;
    }

    // Encrypt
    unsigned char* encrypted = (unsigned char*)malloc(payload_len);
    if (!encrypted) {
        Serial.println("ERR: malloc encrypted");
        throw std::runtime_error("malloc encrypted failed");
    }
    encrypt(key, iv, (int)payload_len, padded.c_str(), encrypted);

    // "Salted__" header + 8-byte salt + ciphertext
    const unsigned char SALTED_HDR[8] = { 'S','a','l','t','e','d','_','_' };
    size_t salted_len = 8 + 8 + payload_len;
    unsigned char* salted = (unsigned char*)malloc(salted_len);
    if (!salted) {
        Serial.println("ERR: malloc salted");
        free(encrypted);
        throw std::runtime_error("malloc salted failed");
    }
    memcpy(salted, SALTED_HDR, 8);
    memcpy(salted + 8, salt, 8);
    memcpy(salted + 16, encrypted, payload_len);

    // Build full HTTP URL with urlsafe base64
    String preparedURL = baseURLATM + "?atm=1&p=" + toBase64(salted, salted_len, BASE64_URLSAFE);

    Serial.println(preparedURL);
    String bech32_lnurl = lnurl_encode(preparedURL);  // prints LNURL and sets qrData

    free(salted);
    free(encrypted);
    return bech32_lnurl;
}


String makeLNURLLegacy(String secretATM, float total, String baseURLATM) {
    ////////////////////////////////////////////
    ///////////////LNURL STUFF//////////////////
    ////USING STEPAN SNIGREVS GREAT CRYTPO//////
    ////////////THANK YOU STEPAN////////////////
    ////////////////////////////////////////////
    String preparedURL = baseURLATM + "?atm=1&p=";

    int randomPin = random(1000, 9999);
    // lnurldevice extension: XOR-HMAC encryption (legacy)
    byte nonce[8];
    for (int i = 0; i < 8; i++)
    {
        nonce[i] = random(256);
    }
    byte payload[51]; // 51 bytes is max one can get with xor-encryption
    size_t payload_len = xor_encrypt(payload, sizeof(payload), (uint8_t*)secretATM.c_str(), secretATM.length(), nonce, sizeof(nonce), randomPin, float(total));
    preparedURL += toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);
    Serial.println(preparedURL);
    String bech32_lnurl = lnurl_encode(preparedURL);  // prints LNURL and sets qrData
    return bech32_lnurl;
}


// --- Safe LNURL bech32 encode (no fixed 200-byte buffer) ---
String lnurl_encode(const String& preparedURL) {
    size_t url_len = preparedURL.length();
    if (url_len == 0) {
        throw std::runtime_error("ERR: empty URL");
    }

    // Copy URL to C buffer
    char* url = (char*)malloc(url_len + 1);
    if (!url) {
        throw std::runtime_error("ERR: malloc url");
    }
    preparedURL.toCharArray(url, url_len + 1);

    // Convert 8-bit bytes to 5-bit groups (bech32 data part).
    // Worst-case output is about ceil(8/5 * url_len) -> ~1.6x, allocate extra headroom.
    size_t data_cap = url_len * 2 + 16;
    byte* data = (byte*)malloc(data_cap);
    if (!data) {
        free(url);
        throw std::runtime_error("ERR: malloc data");
    }
    size_t data_len = 0;
    int ok = convert_bits(data, &data_len, 5, (byte*)url, url_len, 8, 1);
    if (ok != 1 || data_len == 0) {
        free(data);
        free(url);
        throw std::runtime_error("ERR: convert_bits failed");
    }

    // Bech32 output length ~= hrp(5) + '1'(1) + data_len + checksum(6)
    // Add margin +1 for null.
    size_t lnurl_cap = 5 + 1 + data_len + 6 + 32;
    char* charLnurl = (char*)malloc(lnurl_cap);
    if (!charLnurl) {
        free(data);
        free(url);
        throw std::runtime_error("ERR: malloc lnurl");
    }

    int enc_len = bech32_encode(charLnurl, "lnurl", data, data_len);
    if (enc_len <= 0) {
        free(charLnurl);
        free(data);
        free(url);
        throw std::runtime_error("ERR: bech32_encode failed");
    }

    // Uppercase for QR readability (if your bech32 impl returns lowercase)
    to_upper(charLnurl);

    String bech32_lnurl = String(charLnurl);
    free(charLnurl);
    free(data);
    free(url);
    return bech32_lnurl;
}

int xor_encrypt(uint8_t* output, size_t outlen, uint8_t* key, size_t keylen, uint8_t* nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents)
{
    // check we have space for all the data:
    // <variant_byte><len|nonce><len|payload:{pin}{amount}><hmac>
    if (outlen < 2 + nonce_len + 1 + lenVarInt(pin) + 1 + lenVarInt(amount_in_cents) + 8)
    {
        throw std::runtime_error("xor_encrypt not enough space??");
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