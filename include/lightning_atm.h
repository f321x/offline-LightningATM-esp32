#ifndef LIGHTNING_ATM_H
#define LIGHTNING_ATM_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "qrcode.h"
#include "Bitcoin.h"
#include <stdlib.h>
#include <Hash.h>
#include <ctype.h>

// ########################################
// ###########    USER ACTION   ###########
// ########################################
// Generate and copy in LNbits with the LNURLDevice extension the string for the ATM and paste it here:
const String lnurlDeviceString = "https://legend.lnbits.com/lnurldevice/api/v1/lnurl/idexample,keyexample,EUR";
// ########################################
// ########################################
// ########################################

// select the display class and display driver class in the following file (new style):
// 1.54 inch Waveshare e-paper display is "GxEPD2_DRIVER_CLASS GxEPD2_150_BN"
// Waveshare 264x176, 2.7inch E-Ink display - Version 1 = "GxEPD2_DRIVER_CLASS GxEPD2_270"
// Waveshare 264x176, 2.7inch E-Ink display - Version 2 = "GxEPD2_DRIVER_CLASS GxEPD2_270_GDEY027T91"
// Waveshare 2.13 inch e-paper display is "GxEPD2_DRIVER_CLASS GxEPD2_213_flex"
// use search to find the correct line, and uncomment the other display drivers in this header file:
#include "GxEPD2_display_selection_new_style.h"

//  OTHER OPTIONS:

// Activate for debugging over Serial (1), deactivate in production use (0)
#define DEBUG_MODE 0

#define COIN_PIN 17
#define PULSE_TIMEOUT 200
#define LED_BUTTON_PIN 13
#define BUTTON_PIN 32
#define MOSFET_PIN 12
#define QR_VERSION 20

typedef struct s_qrdata
{
	uint8_t current_y;
	uint8_t current_x;
	uint8_t start_x;
	uint8_t start_y;
	uint8_t module_size;
	uint8_t qr_size;
} t_qrdata;

extern String baseURLATM;
extern String secretATM;
extern String currencyATM;

// put function declarations here:
void clean_screen();
void initialize_display();
void to_upper(char* arr);
void qr_withdrawl_screen(String top_message, String bottom_message, const char* qr_content);
char* makeLNURL(float total);
int xor_encrypt(uint8_t* output, size_t outlen, uint8_t* key, size_t keylen, uint8_t* nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents);
void show_inserted_amount(int amount_in_cents);
String get_amount_string(int amount_in_cents);
unsigned int detect_coin();
void home_screen();
void IRAM_ATTR button_pressed_itr();
void wait_for_user_to_scan();
String getValue(String data, char separator, int index);
void display_sleep();
void test_macro();

// Waveshare 1.54 inch e-ink display functions
void home_screen_waveshare_1_54();
void show_inserted_amount_waveshare_1_54(String amount_in_euro);
void qr_withdrawl_screen_waveshare_1_54(String top_message, String bottom_message, const char* qr_content);
void clean_screen_waveshare_1_54();

// Waveshare 2.7 inch e-ink display functions
void home_screen_waveshare_2_7();
void show_inserted_amount_waveshare_2_7(String amount_in_euro);
void qr_withdrawl_screen_waveshare_2_7(String top_message, String bottom_message, const char* qr_content);
void clean_screen_waveshare_2_7();

// Waveshare 2.13 inch e-ink display functions
void home_screen_waveshare_2_13();
void show_inserted_amount_waveshare_2_13(String amount_in_euro);
void qr_withdrawl_screen_waveshare_2_13(String top_message, String bottom_message, const char* qr_content);
void clean_screen_waveshare_2_13();

#endif
