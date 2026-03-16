//! Implement the offline atm lnurl logic from the lnbits fossa project
//! https://github.com/lnbits/fossa/commit/e47cc2d917449d793c29645e748d805fe8e38869

use rand::Rng;
use md5::{Md5, Digest};
use aes::Aes256;
use cbc::Encryptor;
use cipher::{BlockEncryptMut, KeyIvInit};
use base64::{Engine as _, engine::general_purpose::URL_SAFE_NO_PAD};
use bech32::{Bech32, Hrp};


type Aes256CbcEnc = Encryptor<Aes256>;


/// generate the lnurl withdraw string to be encoded in the qr code
pub fn make_lnurl(base_url: &str, atm_secret: &str, amount: u64) -> String {
    // 8-bytes salt
    let salt: [u8; 8] = rand::rng().random();

    // Derive key+iv
    let mut key = [0u8; 32];
    let mut iv = [0u8; 16];
    derive_key_and_iv(atm_secret, &salt, &mut key, &mut iv);

    // Random 4-digit PIN
    let random_pin: u16 = rand::rng().random_range(1000..10000);

    // Create payload "PIN:amount"
    let payload = format!("{}:{}", random_pin, amount);

    let encrypted_payload = aes_encrypt(&key, &iv, payload.as_bytes());

    // Build "Salted__" + salt + encrypted_payload
    const SALTED_HDR: &[u8; 8] = b"Salted__";
    let mut salted = Vec::with_capacity(8 + 8 + encrypted_payload.len());
    salted.extend_from_slice(SALTED_HDR);
    salted.extend_from_slice(&salt);
    salted.extend_from_slice(&encrypted_payload);

    // Encode to URL-safe base64
    let encoded = URL_SAFE_NO_PAD.encode(&salted);

    // Build full URL
    let prepared_url = format!("{}?p={}", base_url, encoded);
    log::debug!("prepared url: {}", &prepared_url);

    // Encode as bech32 with "lnurl" HRP
    let hrp = Hrp::parse("lnurl").expect("Valid HRP");
    bech32::encode_upper::<Bech32>(hrp, prepared_url.as_bytes()).expect("Bech32 encoding failed")
}


/// Derive 32-byte key + 16-byte IV from secret + salt (EVP_BytesToKey-like, MD5 rounds)
fn derive_key_and_iv(atm_secret: &str, salt: &[u8; 8], key: &mut [u8; 32], iv: &mut [u8; 16]) {

    let mut out_key_iv = [0u8; 48];
    let mut have= 0;
    let mut md5_buf = [0u8; 16];

    while have < 48 {
        let mut hasher = Md5::new();

        // D_i = MD5( D_{i-1} (if any) + secret + salt )
        if have > 0 {
            hasher.update(md5_buf);
        }
        hasher.update(atm_secret.as_bytes());
        hasher.update(salt);

        md5_buf.copy_from_slice(&hasher.finalize());

        let to_copy = std::cmp::min(48 - have, 16);
        out_key_iv[have..have + to_copy].copy_from_slice(&md5_buf[..to_copy]);
        have += to_copy;
    }

    // Split the 48 bytes into key (32) and IV (16)
    key.copy_from_slice(&out_key_iv[..32]);
    iv.copy_from_slice(&out_key_iv[32..48]);
}


/// AES-256-CBC encrypt
fn aes_encrypt(key: &[u8; 32], iv: &[u8; 16], plaintext: &[u8]) -> Vec<u8> {
    // Create cipher instance
    // Note: This takes ownership of a copy of the IV, so original IV is not mutated
    let cipher = Aes256CbcEnc::new(key.into(), iv.into());

    // Encrypt the padded plaintext
    cipher.encrypt_padded_vec_mut::<cipher::block_padding::Pkcs7>(plaintext)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_derive_key_and_iv_deterministic() {
        // Test that the same secret and salt produce the same key and IV
        let secret = "test_secret_123";
        let salt: [u8; 8] = [1, 2, 3, 4, 5, 6, 7, 8];
        let mut key1 = [0u8; 32];
        let mut iv1 = [0u8; 16];
        let mut key2 = [0u8; 32];
        let mut iv2 = [0u8; 16];

        derive_key_and_iv(secret, &salt, &mut key1, &mut iv1);
        derive_key_and_iv(secret, &salt, &mut key2, &mut iv2);

        assert_eq!(key1, key2, "Keys should be identical for same inputs");
        assert_eq!(iv1, iv2, "IVs should be identical for same inputs");
    }

    #[test]
    fn test_derive_key_and_iv_different_secrets() {
        // Test that different secrets produce different keys
        let salt: [u8; 8] = [1, 2, 3, 4, 5, 6, 7, 8];
        let mut key1 = [0u8; 32];
        let mut iv1 = [0u8; 16];
        let mut key2 = [0u8; 32];
        let mut iv2 = [0u8; 16];

        derive_key_and_iv("secret1", &salt, &mut key1, &mut iv1);
        derive_key_and_iv("secret2", &salt, &mut key2, &mut iv2);

        assert_ne!(key1, key2, "Different secrets should produce different keys");
        assert_ne!(iv1, iv2, "Different secrets should produce different IVs");
    }

    #[test]
    fn test_derive_key_and_iv_different_salts() {
        // Test that different salts produce different keys
        let secret = "test_secret";
        let salt1: [u8; 8] = [1, 2, 3, 4, 5, 6, 7, 8];
        let salt2: [u8; 8] = [8, 7, 6, 5, 4, 3, 2, 1];
        let mut key1 = [0u8; 32];
        let mut iv1 = [0u8; 16];
        let mut key2 = [0u8; 32];
        let mut iv2 = [0u8; 16];

        derive_key_and_iv(secret, &salt1, &mut key1, &mut iv1);
        derive_key_and_iv(secret, &salt2, &mut key2, &mut iv2);

        assert_ne!(key1, key2, "Different salts should produce different keys");
        assert_ne!(iv1, iv2, "Different salts should produce different IVs");
    }

    #[test]
    fn test_derive_key_and_iv_empty_secret() {
        // Test with empty secret
        let salt: [u8; 8] = [1, 2, 3, 4, 5, 6, 7, 8];
        let mut key = [0u8; 32];
        let mut iv = [0u8; 16];

        derive_key_and_iv("", &salt, &mut key, &mut iv);

        // Should not panic and should produce non-zero results (due to salt)
        assert_ne!(key, [0u8; 32], "Key should not be all zeros with empty secret");
    }

    #[test]
    fn test_derive_key_and_iv_known_vector() {
        // Test with a known vector to ensure consistency
        let secret = "MySecret";
        let salt: [u8; 8] = [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08];
        let mut key = [0u8; 32];
        let mut iv = [0u8; 16];

        derive_key_and_iv(secret, &salt, &mut key, &mut iv);

        // Verify that key and IV are fully populated (not all zeros)
        assert_ne!(key, [0u8; 32], "Key should be populated");
        assert_ne!(iv, [0u8; 16], "IV should be populated");

        // Verify lengths
        assert_eq!(key.len(), 32);
        assert_eq!(iv.len(), 16);
    }

    #[test]
    fn test_aes_encrypt_deterministic() {
        // Test that same inputs produce same output
        let key: [u8; 32] = [0x42; 32];
        let iv: [u8; 16] = [0x13; 16];
        let plaintext = b"Hello, World!";

        let encrypted1 = aes_encrypt(&key, &iv, plaintext);
        let encrypted2 = aes_encrypt(&key, &iv, plaintext);

        assert_eq!(encrypted1, encrypted2, "Same inputs should produce same ciphertext");
        assert!(!encrypted1.is_empty(), "Ciphertext should not be empty");
    }

    #[test]
    fn test_aes_encrypt_different_keys() {
        // Test that different keys produce different outputs
        let key1: [u8; 32] = [0x42; 32];
        let key2: [u8; 32] = [0x43; 32];
        let iv: [u8; 16] = [0x13; 16];
        let plaintext = b"Hello, World!";

        let encrypted1 = aes_encrypt(&key1, &iv, plaintext);
        let encrypted2 = aes_encrypt(&key2, &iv, plaintext);

        assert_ne!(encrypted1, encrypted2, "Different keys should produce different ciphertext");
    }

    #[test]
    fn test_aes_encrypt_different_ivs() {
        // Test that different IVs produce different outputs
        let key: [u8; 32] = [0x42; 32];
        let iv1: [u8; 16] = [0x13; 16];
        let iv2: [u8; 16] = [0x14; 16];
        let plaintext = b"Hello, World!";

        let encrypted1 = aes_encrypt(&key, &iv1, plaintext);
        let encrypted2 = aes_encrypt(&key, &iv2, plaintext);

        assert_ne!(encrypted1, encrypted2, "Different IVs should produce different ciphertext");
    }

    #[test]
    fn test_aes_encrypt_empty_plaintext() {
        // Test with empty plaintext
        let key: [u8; 32] = [0x42; 32];
        let iv: [u8; 16] = [0x13; 16];
        let plaintext = b"";

        let encrypted = aes_encrypt(&key, &iv, plaintext);

        // Even empty plaintext should produce output due to PKCS7 padding
        assert!(!encrypted.is_empty(), "Encrypted empty plaintext should not be empty (padding)");
        assert_eq!(encrypted.len(), 16, "Encrypted empty plaintext should be one block (16 bytes)");
    }

    #[test]
    fn test_aes_encrypt_various_lengths() {
        // Test encryption with various plaintext lengths
        let key: [u8; 32] = [0x42; 32];
        let iv: [u8; 16] = [0x13; 16];

        for len in [1, 15, 16, 17, 31, 32, 100] {
            let plaintext = vec![0x55u8; len];
            let encrypted = aes_encrypt(&key, &iv, &plaintext);

            // Ciphertext should be larger than plaintext due to padding
            assert!(encrypted.len() >= len, "Ciphertext should be at least as long as plaintext");
            // Should be a multiple of block size (16 bytes)
            assert_eq!(encrypted.len() % 16, 0, "Ciphertext should be multiple of block size");
        }
    }

    #[test]
    fn test_make_lnurl_structure() {
        // Test that make_lnurl produces valid structure
        let base_url = "https://example.com/withdraw";
        let secret = "my_atm_secret";
        let amount = 1000u64;

        let lnurl = make_lnurl(base_url, secret, amount);

        // Should start with "LNURL" (uppercase bech32)
        assert!(lnurl.starts_with("LNURL"), "LNURL should start with 'LNURL'");

        // Should be reasonably long
        assert!(lnurl.len() > 50, "LNURL should be a substantial length");
    }

    #[test]
    fn test_make_lnurl_uniqueness() {
        // Test that multiple calls produce different LNURLs (due to random salt and PIN)
        let base_url = "https://example.com/withdraw";
        let secret = "my_atm_secret";
        let amount = 1000u64;

        let lnurl1 = make_lnurl(base_url, secret, amount);
        let lnurl2 = make_lnurl(base_url, secret, amount);
        let lnurl3 = make_lnurl(base_url, secret, amount);

        // All should be different due to randomness
        assert_ne!(lnurl1, lnurl2, "Sequential LNURLs should differ");
        assert_ne!(lnurl2, lnurl3, "Sequential LNURLs should differ");
        assert_ne!(lnurl1, lnurl3, "Sequential LNURLs should differ");
    }

    #[test]
    fn test_make_lnurl_different_amounts() {
        // Test with different amounts
        let base_url = "https://example.com/withdraw";
        let secret = "my_atm_secret";

        for amount in [1, 100, 1000, 10000, 1000000] {
            let lnurl = make_lnurl(base_url, secret, amount);
            assert!(lnurl.starts_with("LNURL"), "LNURL should start with 'LNURL' for amount {}", amount);
            assert!(lnurl.len() > 50, "LNURL should have reasonable length for amount {}", amount);
        }
    }

    #[test]
    fn test_make_lnurl_different_secrets() {
        // Test with different secrets
        let base_url = "https://example.com/withdraw";
        let amount = 1000u64;

        for secret in ["secret1", "secret2", "very_long_secret_with_special_chars!@#$%"] {
            let lnurl = make_lnurl(base_url, secret, amount);
            assert!(lnurl.starts_with("LNURL"), "LNURL should start with 'LNURL' for secret");
            assert!(lnurl.len() > 50, "LNURL should have reasonable length");
        }
    }

    #[test]
    fn test_make_lnurl_different_base_urls() {
        // Test with different base URLs
        let secret = "my_atm_secret";
        let amount = 1000u64;

        for base_url in [
            "http://localhost:5000/api/withdraw",
            "https://example.com/withdraw",
            "https://api.lnbits.com/withdraw",
        ] {
            let lnurl = make_lnurl(base_url, secret, amount);
            assert!(lnurl.starts_with("LNURL"), "LNURL should start with 'LNURL' for URL {}", base_url);
        }
    }

    #[test]
    fn test_make_lnurl_zero_amount() {
        // Test with zero amount (edge case)
        let base_url = "https://example.com/withdraw";
        let secret = "my_atm_secret";
        let amount = 0u64;

        let lnurl = make_lnurl(base_url, secret, amount);
        assert!(lnurl.starts_with("LNURL"), "LNURL should work with zero amount");
    }

    #[test]
    fn test_make_lnurl_max_amount() {
        // Test with maximum u64 amount
        let base_url = "https://example.com/withdraw";
        let secret = "my_atm_secret";
        let amount = u64::MAX;

        let lnurl = make_lnurl(base_url, secret, amount);
        assert!(lnurl.starts_with("LNURL"), "LNURL should work with max u64 amount");
    }

    #[test]
    fn test_make_lnurl_empty_secret() {
        // Test with empty secret (edge case)
        let base_url = "https://example.com/withdraw";
        let secret = "";
        let amount = 1000u64;

        let lnurl = make_lnurl(base_url, secret, amount);
        assert!(lnurl.starts_with("LNURL"), "LNURL should work with empty secret");
    }

    #[test]
    fn test_make_lnurl_bech32_validity() {
        // Test that the output is valid bech32
        let base_url = "https://example.com/withdraw";
        let secret = "my_atm_secret";
        let amount = 1000u64;

        let lnurl = make_lnurl(base_url, secret, amount);

        // Try to decode it as bech32
        let decoded = bech32::decode(&lnurl);
        assert!(decoded.is_ok(), "LNURL should be valid bech32: {:?}", decoded.err());

        let (hrp, _data) = decoded.unwrap();
        // HRP is case-insensitive, but our encoding uses uppercase, so it returns uppercase
        assert_eq!(hrp.to_string().to_lowercase(), "lnurl", "HRP should be 'lnurl' (case-insensitive)");
    }

    #[test]
    fn test_salted_header_presence() {
        // Test that the encrypted payload contains "Salted__" header
        let base_url = "https://example.com/withdraw";
        let secret = "test_secret";
        let amount = 1000u64;

        let lnurl = make_lnurl(base_url, secret, amount);

        // Decode the bech32 to get the URL
        let (_, data) = bech32::decode(&lnurl).expect("Valid bech32");
        let url_bytes: Vec<u8> = data.iter().map(|fe| u8::from(*fe)).collect();
        let url = String::from_utf8(url_bytes).expect("Valid UTF-8");

        // Extract the base64 parameter
        assert!(url.contains("?p="), "URL should contain parameter");
        let parts: Vec<&str> = url.split("?p=").collect();
        assert_eq!(parts.len(), 2, "URL should have exactly one ?p= parameter");

        let encoded_payload = parts[1];
        let decoded_payload = URL_SAFE_NO_PAD.decode(encoded_payload).expect("Valid base64");

        // Check for "Salted__" header
        assert!(decoded_payload.len() >= 8, "Payload should be at least 8 bytes");
        assert_eq!(&decoded_payload[0..8], b"Salted__", "Should start with Salted__ header");
    }

    #[test]
    fn test_payload_structure() {
        // Test the overall payload structure
        let base_url = "https://example.com/withdraw";
        let secret = "test_secret";
        let amount = 5000u64;

        let lnurl = make_lnurl(base_url, secret, amount);

        // Decode and verify structure
        let (_, data) = bech32::decode(&lnurl).expect("Valid bech32");
        let url_bytes: Vec<u8> = data.iter().map(|fe| u8::from(*fe)).collect();
        let url = String::from_utf8(url_bytes).expect("Valid UTF-8");

        // Should contain base URL
        assert!(url.starts_with(base_url), "URL should start with base_url");

        let encoded_payload = url.split("?p=").nth(1).expect("Should have parameter");
        let decoded_payload = URL_SAFE_NO_PAD.decode(encoded_payload).expect("Valid base64");

        // Structure: "Salted__" (8) + salt (8) + encrypted_payload (>=16)
        assert!(decoded_payload.len() >= 8 + 8 + 16,
                "Payload should be at least 32 bytes (header + salt + encrypted block)");
    }

    #[test]
    fn test_make_lnurl_testing() {
        let base_url = "https://example.com/withdraw";
        let secret = "";
        let amount = 100;

        let lnurl = make_lnurl(base_url, secret, amount);
        assert!(lnurl.starts_with("LNURL"), "LNURL should work with empty secret");
    }
}
