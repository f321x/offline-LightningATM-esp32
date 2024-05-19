#include "lightning_atm.h"

const unsigned int COINS[] = { 0, 0, 5, 10, 20, 50, 100, 200, 1, 2 };
bool button_pressed = false;
unsigned int inserted_cents = 0;
unsigned long long time_last_press = millis();
LnurlPoS lnurl_utils(lnurlDeviceString, DEBUG_MODE);
String currencyATM = lnurl_utils.getCurrency();

// *** for Waveshare ESP32 Driver board *** //
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
SPIClass hspi(HSPI);
#endif
// *** end Waveshare ESP32 Driver board *** //

BlockClock* blockClock;

void setup()
{
  initialize_display(); // connection to the e-ink display
  Serial.begin(9600);
  // *** for Waveshare ESP32 Driver board *** //
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
  hspi.begin(13, 12, 14, 15); // remap hspi for EPD (swap pins)
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#endif
  // *** end Waveshare ESP32 Driver board *** //
  if (DEBUG_MODE)                                           // serial connection for debugging over USB
  {
    sleep(3);
    Serial.println("Setup with debug mode...");             // for monitoring with serial monitor to debug
    Serial.println("Selected display type: " + display_type);
  }
  pinMode(COIN_PIN, INPUT_PULLUP);                          // coin acceptor input
  pinMode(LED_BUTTON_PIN, OUTPUT);                          // LED of the LED Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);                        // Button
  pinMode(MOSFET_PIN, OUTPUT);                              // mosfet relay to block the coin acceptor
  digitalWrite(MOSFET_PIN, LOW);                            // set it low to accept coins, high to block coins
  attachInterrupt(BUTTON_PIN, button_pressed_itr, FALLING); // interrupt, will set button_pressed to true when button is pressed
  home_screen();                                            // will show first screen
  digitalWrite(LED_BUTTON_PIN, HIGH);                       // light up the led

  if (BLOCKCLOCK_ACTIVE)
  {
    try {
      blockClock = new BlockClock(currencyATM.c_str(), WIFI_SSID, WIFI_PW, DEBUG_MODE);
    }
    catch (const std::exception& e) {
      Serial.println(e.what());
      BLOCKCLOCK_ACTIVE = false; // disable blockclock if there is an error
    }
  }
}

void loop()
{
  while (true && BLOCKCLOCK_ACTIVE) {  // testing/demonstration function of Blockclock
    print_blockclock_homescreen();
  }

  unsigned int pulses = 0;
  unsigned long long time_last_press;

  pulses = detect_coin(); // detect_coin() is a loop to detect the input of coins, will return the amount of pulses
  if (pulses >= 2 && pulses <= 9)
  {
    digitalWrite(MOSFET_PIN, HIGH);
    digitalWrite(LED_BUTTON_PIN, LOW);
    inserted_cents += COINS[pulses];
    show_inserted_amount(inserted_cents);
  }
  else if (button_pressed && inserted_cents > 0)
  {
    digitalWrite(MOSFET_PIN, HIGH);
    button_pressed = false;
    char* lnurl = lnurl_utils.makeLNURL(inserted_cents);
    qr_withdrawl_screen(lnurl);
    free(lnurl);
    wait_for_user_to_scan();
    digitalWrite(LED_BUTTON_PIN, HIGH);
    home_screen();
    digitalWrite(MOSFET_PIN, LOW);
    inserted_cents = 0;
  }
  else if (button_pressed && !pulses && !inserted_cents) // to clean the screen (for storage), press the button several times
  {
    int press_counter = 0;

    button_pressed = false;
    time_last_press = millis();
    while ((millis() - time_last_press) < 4000 && press_counter < 6)
    {
      if (button_pressed)
      {
        if (DEBUG_MODE)
          Serial.println("Button pressed");
        time_last_press = millis();
        button_pressed = false;
        press_counter++;
        delay(500);
      }
    }
    if (press_counter > 5)
    {
      if (DEBUG_MODE)
        Serial.println("Button pressed over 5 times, will clean screen...");
      digitalWrite(LED_BUTTON_PIN, LOW);
      clean_screen();
      display_sleep();
      delay(30000);
      home_screen();
    }
  }
}

// function to handle the button interrupt
void IRAM_ATTR button_pressed_itr()
{
  button_pressed = true;
}

// blocking loop which is called when the qr code is shown
void wait_for_user_to_scan()
{
  unsigned long long time;
  bool light_on;

  if (DEBUG_MODE)
    Serial.println("Waiting for user to scan qr code and press button...");
  light_on = true;
  time = millis();  // save start time
  digitalWrite(LED_BUTTON_PIN, HIGH);  // light up the led
  delay(5000);
  button_pressed = false;
  Serial.println("wait for user to press button or 10 minutes to go back to home screen");
  while (!button_pressed && (millis() - time) < 600000)
  {
    if (!light_on && (millis() - time) > 30000)
    {
      digitalWrite(LED_BUTTON_PIN, HIGH);
      light_on = true;
    }
    else if (light_on && (millis() - time) > 30000)
    {
      digitalWrite(LED_BUTTON_PIN, LOW);
      light_on = false;
    }
    delay(500);
  }
  Serial.println("Exit waiting");
}

unsigned int detect_coin()
{
  unsigned long last_pulse;
  unsigned int pulses;
  bool prev_value;
  bool read_value;
  unsigned long entering_time;
  unsigned long current_time;

  if (DEBUG_MODE)
    Serial.println("Starting coin detection...");
  pulses = 0;
  read_value = 1;
  prev_value = digitalRead(COIN_PIN);
  digitalWrite(LED_BUTTON_PIN, HIGH);
  digitalWrite(MOSFET_PIN, LOW);
  button_pressed = false;
  entering_time = millis();
  while (true && !button_pressed)
  {
    read_value = digitalRead(COIN_PIN);
    if (read_value != prev_value && !read_value)
    {
      delay(35);
      read_value = digitalRead(COIN_PIN);
      if (!read_value)
      {
        pulses++;
        last_pulse = millis();
      }
    }
    prev_value = read_value;
    current_time = millis();
    if (pulses > 0 && (current_time - last_pulse > PULSE_TIMEOUT))
    {
      break;
    }
    else if (pulses == 0 && ((current_time - entering_time) > 43200000) // refreshes the screen every 12h
      && inserted_cents == 0)
    {
      clean_screen();
      delay(10000);
      entering_time = millis();
      home_screen();
    }
    else if (inserted_cents > 0 && (current_time - entering_time) > 360000) // break the loop if no new coin is inserted for some time
      button_pressed = true;
  }
  if (DEBUG_MODE)
    Serial.println("Pulses: " + String(pulses));
  if (button_pressed)
  {
    if (DEBUG_MODE)
      Serial.println("Button pressed, stopping coin detection");
    return (0);
  }
  return (pulses);
}

/*
** DISPLAY UTILS
*/

// dummy function to showcase blockClock class
void print_blockclock_homescreen() {
  uint block_height = blockClock->getBlockHeight();
  uint exchange_rate = blockClock->getExchangeRate();

  if (block_height == 0 || exchange_rate == 0) {
    if (DEBUG_MODE)
      Serial.println("Error fetching block height or exchange rate");
    return;
  }
  Serial.println(block_height);
  delay(5000);
  Serial.println(exchange_rate);
  delay(5000);
}

// sleep is to put the screen in hibernation mode for longer static frames
void display_sleep()
{
  if (display_type == "GxEPD2_150_BN")
    display.hibernate();
  else if (display_type == "GxEPD2_270")
    display.hibernate();
  else if (display_type == "GxEPD2_270_GDEY027T91")
    display.hibernate();
  else if (display_type == "GxEPD2_213_B74")
    display.hibernate();
  else if (display_type == "GxEPD2_213_flex")
    display.hibernate();
  else
    Serial.println("No suitable display class defined.");
}

void initialize_display()
{
  if (display_type == "GxEPD2_150_BN")
    display.init(115200, true, 2, false);
  else if (display_type == "GxEPD2_270")
    display.init(115200, true, 2, false);
  else if (display_type == "GxEPD2_270_GDEY027T91")
    display.init(115200, true, 2, false);
  else if (display_type == "GxEPD2_213_B74")
    display.init(115200, true, 2, false);
  else if (display_type == "GxEPD2_213_flex")
    display.init(115200, true, 2, false);
  else
    Serial.println("No suitable display class defined.");
}

void home_screen()
{
  if (display_type == "GxEPD2_150_BN")
    home_screen_waveshare_1_54();
  else if (display_type == "GxEPD2_270")
    home_screen_waveshare_2_7();
  else if (display_type == "GxEPD2_270_GDEY027T91")
    home_screen_waveshare_2_7();
  else if (display_type == "GxEPD2_213_B74")
    home_screen_waveshare_2_13();
  else if (display_type == "GxEPD2_213_flex")
    home_screen_waveshare_2_13_flex();
  else
    Serial.println("No suitable display class defined.");
  if (DEBUG_MODE)
    Serial.println("Home screen printed.");
}

void show_inserted_amount(int amount_in_cents)
{
  String amount_in_euro_string;

  amount_in_euro_string = get_amount_string(amount_in_cents);
  if (display_type == "GxEPD2_150_BN")
    show_inserted_amount_waveshare_1_54(amount_in_euro_string);
  else if (display_type == "GxEPD2_270")
    show_inserted_amount_waveshare_2_7(amount_in_euro_string);
  else if (display_type == "GxEPD2_270_GDEY027T91")
    show_inserted_amount_waveshare_2_7(amount_in_euro_string);
  else if (display_type == "GxEPD2_213_B74")
    show_inserted_amount_waveshare_2_13(amount_in_euro_string);
  else if (display_type == "GxEPD2_213_flex")
    show_inserted_amount_waveshare_2_13_flex(amount_in_euro_string);
  else
    Serial.println("No suitable display class defined.");
  if (DEBUG_MODE)
    Serial.println("New amount on screen.");
}

void qr_withdrawl_screen(const char* qr_content)
{
  if (display_type == "GxEPD2_150_BN")
    qr_withdrawl_screen_waveshare_1_54(qr_content);
  else if (display_type == "GxEPD2_270")
    qr_withdrawl_screen_waveshare_2_7(qr_content);
  else if (display_type == "GxEPD2_270_GDEY027T91")
    qr_withdrawl_screen_waveshare_2_7(qr_content);
  else if (display_type == "GxEPD2_213_B74")
    qr_withdrawl_screen_waveshare_2_13(qr_content);
  else if (display_type == "GxEPD2_213_flex")
    qr_withdrawl_screen_waveshare_2_13_flex(qr_content);
  else
    Serial.println("No suitable display class defined.");
  if (DEBUG_MODE)
    Serial.println("QR generated and Withdrawl screen printed.");
}


//  Called to clean the e-ink screen for storage over longer periods 
// by button presses on home screen
void clean_screen()
{
  if (DEBUG_MODE)
    Serial.println("Cleaning screen...");
  if (display_type == "GxEPD2_150_BN")
    clean_screen_waveshare_1_54();
  else if (display_type == "GxEPD2_270")
    clean_screen_waveshare_2_7();
  else if (display_type == "GxEPD2_270_GDEY027T91")
    clean_screen_waveshare_2_7();
  else if (display_type == "GxEPD2_213_B74")
    clean_screen_waveshare_2_13();
  else if (display_type == "GxEPD2_213_flex")
    clean_screen_waveshare_2_13_flex();
  else
    Serial.println("No suitable display class defined.");
}

// converts a cent amount to a String like "1.15 Euro"
String get_amount_string(int amount_in_cents)
{
  String euro;
  String cents;
  String return_value;
  int euro_value;
  int cent_remainder;

  euro_value = amount_in_cents / 100;
  cent_remainder = amount_in_cents % 100;
  euro = String(euro_value);
  if (cent_remainder > 9)
    cents = String(cent_remainder);
  else if (cent_remainder < 10)
    cents = "0" + String(cent_remainder);
  return_value = String(euro) + "." + String(cents) + " " + currencyATM;
  if (DEBUG_MODE)
    Serial.println("Calculated amount string: " + return_value);
  return (return_value);
}

// Display functions for specific display types

// ################################################################
// # Waveshare 1.54 inch e-Paper Display Modul with SPI Interface #
// ################################################################

void home_screen_waveshare_1_54()
{
  if (DEBUG_MODE)
    Serial.println("Home screen for Waveshare 1.54 inch display...");
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(0, 10);
  display.setTextSize(3);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Insert\nEuro coins\non the\nright\nside to\nstart ->");

  display.setCursor(0, 160);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Prepare Lightning enabled Bitcoin\nwallet before starting!\n\nSupported coins:\n5 - 50 Cent and 1 - 2 Euro");
  display.nextPage();
  display.hibernate();
}

void show_inserted_amount_waveshare_1_54(String amount_in_euro)
{
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(0, 4);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Inserted amount:");

  display.setCursor(10, 90);
  display.setTextSize(3);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(amount_in_euro);

  display.setCursor(0, 160);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Press button\n once finished.");

  display.nextPage();
}

void qr_withdrawl_screen_waveshare_1_54(const char* qr_content)
{
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(QR_VERSION)]; // 20 is "qr version"
  t_qrdata qr;

  // initialize qr code data
  qrcode_initText(&qrcoded, qrcodeData, QR_VERSION, 0, qr_content);
  qr.qr_size = qrcoded.size * 2;
  qr.start_x = (150 - qr.qr_size) / 2;
  qr.start_y = (150 - qr.qr_size) / 2;
  qr.module_size = 3;

  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  // loop trough Y and X axis to draw the qr code rectangle by rectangle
  // 0, 0 is the top left of the screen
  for (qr.current_y = 0; qr.current_y < qrcoded.size; qr.current_y++)
  {
    for (qr.current_x = 0; qr.current_x < qrcoded.size; qr.current_x++)
    {
      if (qrcode_getModule(&qrcoded, qr.current_x, qr.current_y))
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y,
          qr.module_size,
          qr.module_size,
          GxEPD_BLACK);
      else
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y,
          qr.module_size,
          qr.module_size,
          GxEPD_WHITE);
    }
  }
  // draw the text messages on the screen
  display.setCursor(0, 4);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Please scan QR:");  // top message
  display.setCursor(0, 170);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Press the \nbutton to reset");  // bottom message
  display.nextPage();
  display.hibernate();
}

void clean_screen_waveshare_1_54()
{
  display.firstPage();
  display.nextPage();
  display.hibernate();
}

// ##############################################
// # Waveshare 2.7 inch e-ink display functions #
// ##############################################

void home_screen_waveshare_2_7()
{
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(11, 20);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Insert Euro coins\n on the right ->\n to start ATM");

  display.drawBitmap(172, 65, bitcoin_logo, 64, 64, GxEPD_BLACK);

  display.setCursor(12, 140);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Prepare Lightning enabled Bitcoin\n  wallet before starting!\n  Supported coins: 5 - 50 Cent, 1 - 2 Euro");

  display.nextPage();
  display.hibernate();
}

void show_inserted_amount_waveshare_2_7(String amount_in_euro)
{
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(11, 10);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Inserted amount:");

  display.setCursor(20, 75);
  display.setTextSize(3);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(amount_in_euro);

  display.setCursor(11, 135);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Press button\n once finished.");

  display.nextPage();
}

void qr_withdrawl_screen_waveshare_2_7(const char* qr_content)
{
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(QR_VERSION)]; // 20 is "qr version"
  t_qrdata qr;

  qrcode_initText(&qrcoded, qrcodeData, QR_VERSION, 0, qr_content);
  qr.qr_size = qrcoded.size * 3;
  qr.start_x = (264 - qr.qr_size) / 2;
  qr.start_y = (176 - qr.qr_size) / 2;
  qr.module_size = 3;

  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  for (qr.current_y = 0; qr.current_y < qrcoded.size; qr.current_y++)
  {
    for (qr.current_x = 0; qr.current_x < qrcoded.size; qr.current_x++)
    {
      if (qrcode_getModule(&qrcoded, qr.current_x, qr.current_y))
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y, qr.module_size, qr.module_size, GxEPD_BLACK);
      else
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y, qr.module_size, qr.module_size, GxEPD_WHITE);
    }
  }
  display.setCursor(11, 5);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Please scan QR code:");  // top message

  display.setCursor(11, 155);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Reset - press button");  // bottom message

  display.nextPage();
  display.hibernate();
}

void clean_screen_waveshare_2_7()
{
  display.firstPage();
  display.nextPage();
  display.hibernate();
}

// #########################################################
// # Waveshare 2.13 inch e-ink display (250x122) functions #
// #########################################################

void home_screen_waveshare_2_13()
{
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(5, 5);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("LIGHTNING ATM");
  display.setCursor(3, 33);
  display.println("Insert coins");
  display.setCursor(3, 50);
  display.println("on the right");
  display.setCursor(3, 67);
  display.println("side to start");

  display.drawBitmap(180, 15, bitcoin_logo, 64, 64, GxEPD_BLACK);

  display.setCursor(0, 95);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Prepare Lightning enabled Bitcoin\n wallet before starting!\n Supported coins: 2 cent to 2 euro");
  display.nextPage();
  display.hibernate();
}

void show_inserted_amount_waveshare_2_13(String amount_in_euro)
{
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(10, 4);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Inserted amount:");

  display.setCursor(35, 45);
  display.setTextSize(3);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(amount_in_euro);

  display.setCursor(0, 85);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Press button\n to show QR code");

  display.nextPage();
}

void qr_withdrawl_screen_waveshare_2_13(const char* qr_content)
{
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(QR_VERSION)]; // 20 is "qr version"
  t_qrdata qr;

  qrcode_initText(&qrcoded, qrcodeData, QR_VERSION, 0, qr_content);
  qr.qr_size = qrcoded.size * 2;
  qr.start_x = (230 - qr.qr_size) / 2;
  qr.start_y = (122 - qr.qr_size) / 2;
  qr.module_size = 2;

  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  for (qr.current_y = 0; qr.current_y < qrcoded.size; qr.current_y++)
  {
    for (qr.current_x = 0; qr.current_x < qrcoded.size; qr.current_x++)
    {
      if (qrcode_getModule(&qrcoded, qr.current_x, qr.current_y))
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y, qr.module_size, qr.module_size, GxEPD_BLACK);
      else
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y, qr.module_size, qr.module_size, GxEPD_WHITE);
    }
  }
  display.setCursor(0, 20);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Scan\n\n  QR\n\n code");  // top message

  display.setCursor(181, 32);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Reset");
  display.setCursor(181, 53);
  display.println("press");
  display.setCursor(176, 77);
  display.println("button");
  display.nextPage();
  display.hibernate();
}

void clean_screen_waveshare_2_13()
{
  display.firstPage();
  display.nextPage();
  display.hibernate();
}

// ###########################################################################
// # Waveshare 2.13 inch e-ink display (D) flex (yellow) (212x104) functions #
// ###########################################################################

void home_screen_waveshare_2_13_flex()
{
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(5, 5);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("LIGHTNING ATM");
  display.setCursor(3, 25);
  display.println("Insert coins");
  display.setCursor(3, 42);
  display.println("on the right");
  display.setCursor(3, 59);
  display.println("side to start");

  display.drawBitmap(151, 8, bitcoin_logo, 64, 64, GxEPD_BLACK);

  display.setCursor(0, 80);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Prepare Lightning enabled Bitcoin\n wallet before starting!\n Supported coins: 2 cent to 2 euro");
  display.nextPage();
  display.hibernate();
}

void show_inserted_amount_waveshare_2_13_flex(String amount_in_euro)
{
  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();

  display.setCursor(0, 4);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Inserted amount:");

  display.setCursor(30, 35);
  display.setTextSize(3);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(amount_in_euro);

  display.setCursor(0, 70);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Press button\n to show QR code");

  display.nextPage();
}

void qr_withdrawl_screen_waveshare_2_13_flex(const char* qr_content)
{
  QRCode qrcoded;
  uint8_t qrcodeData[qrcode_getBufferSize(QR_VERSION)]; // 20 is "qr version"
  t_qrdata qr;

  qrcode_initText(&qrcoded, qrcodeData, QR_VERSION, 0, qr_content);
  qr.qr_size = qrcoded.size * 2;
  qr.start_x = (220 - qr.qr_size) / 2;
  qr.start_y = (105 - qr.qr_size) / 2;
  qr.module_size = 2;

  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  for (qr.current_y = 0; qr.current_y < qrcoded.size; qr.current_y++)
  {
    for (qr.current_x = 0; qr.current_x < qrcoded.size; qr.current_x++)
    {
      if (qrcode_getModule(&qrcoded, qr.current_x, qr.current_y))
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y, qr.module_size, qr.module_size, GxEPD_BLACK);
      else
        display.fillRect(qr.start_x + qr.module_size * qr.current_x,
          qr.start_y + qr.module_size * qr.current_y, qr.module_size, qr.module_size, GxEPD_WHITE);
    }
  }
  display.setCursor(0, 12);
  display.setTextSize(2);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println(" Scan\n\n  QR\n\n code");  // top message

  display.setCursor(170, 37);
  display.setTextSize(1);
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.println("Reset");
  display.setCursor(170, 47);
  display.println("press");
  display.setCursor(167, 57);
  display.println("button");
  display.nextPage();
  display.hibernate();
}

void clean_screen_waveshare_2_13_flex()
{
  display.firstPage();
  display.nextPage();
  display.hibernate();
}