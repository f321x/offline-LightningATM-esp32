#include "lightning_atm.h"
#include <mbedtls/aes.h>
#include <MD5Builder.h>
#include <math.h>
#include "FS.h"
#include "SPIFFS.h"

const unsigned int COINS[] = { 0, 0, 5, 10, 20, 50, 100, 200, 1, 2 };
bool button_pressed = false;
unsigned int inserted_cents = 0;
unsigned long long time_last_press = millis();
String baseURLATM;
String secretATM;
String currencyATM;
bool fossaMode = false;  // set automatically from storedDeviceString in setup()
String storedDeviceString = "";  // loaded from SPIFFS config.json at boot

// ─── SPIFFS / serial-config helpers ──────────────────────────────────────────

static void serialWriteChunked(const String& msg) {
  const int chunkSize = 64;
  for (unsigned int i = 0; i < msg.length(); i += chunkSize) {
    unsigned int end = min((unsigned int)(i + chunkSize), (unsigned int)msg.length());
    Serial.write(msg.c_str() + i, end - i);
    Serial.flush();
    delay(15);
  }
}

static String readFileContent(const char* path) {
  if (!SPIFFS.exists(path)) return "";
  File f = SPIFFS.open(path, "r");
  if (!f) return "";
  String content = f.readString();
  f.close();
  return content;
}

// Extract value for key "deviceString" from installer JSON:
// [{"name":"ssid","value":""},{"name":"wifipassword","value":""},{"name":"deviceString","value":"..."}]
static String extractDeviceStringFromJson(const String& json) {
  int nameIdx = json.indexOf("\"deviceString\"");
  if (nameIdx < 0) return "";
  int valueIdx = json.indexOf("\"value\":\"", nameIdx);
  if (valueIdx < 0) return "";
  int start = valueIdx + 9;
  int end = json.indexOf("\"", start);
  if (end <= start) return "";
  return json.substring(start, end);
}

// ─── Forward declarations ─────────────────────────────────────────────────────
void configMode();
void boot_info_screen();

// *** for Waveshare ESP32 Driver board *** //
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
SPIClass hspi(HSPI);
#endif
// *** end Waveshare ESP32 Driver board *** //


// ─── Boot info screen ────────────────────────────────────────────────────────
// Shows firmware version, detected mode, currency and (truncated) base URL
// on the e-paper display for ~5 seconds right after booting with a stored config.
void boot_info_screen()
{
  // FW: first 4 chars + ".." + last 8 chars  →  e.g. "v938..7-v1-rc1"
  String verStr = String(VERSION);
  String shortVer = (verStr.length() > 12)
    ? (verStr.substring(0, 4) + ".." + verStr.substring(verStr.length() - 8))
    : verStr;

  String modeStr = fossaMode ? "Ext. FOSSA" : "Ext. LNURL";

  bool is27 = (display_type == "GxEPD2_270" || display_type == "GxEPD2_270_GDEY027T91");
  bool is154 = (display_type == "GxEPD2_150_BN");

  // URL truncation: size-2 font → 12px/char
  // 2.7" (264px): 20; 2.13" (250px): 18; 1.54" (200px): 14
  int urlMaxChars = is27 ? 20 : (is154 ? 14 : 18);
  String shortURL = (baseURLATM.length() > (unsigned)urlMaxChars)
    ? baseURLATM.substring(0, urlMaxChars) + ".."
    : baseURLATM;

  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
  display.setTextSize(2);

  if (is27) {
    // 2.7" = 264x176 px, 5 rows à size-2 (16px), step 32px
    display.setCursor(0, 20);  display.print("BOOT CONFIG");
    display.setCursor(0, 52);  display.print("FW: "); display.print(shortVer);
    display.setCursor(0, 84);  display.print(modeStr);
    display.setCursor(0, 116); display.print("Currency: "); display.print(currencyATM);
    display.setCursor(0, 148); display.print(shortURL);
  }
  else if (is154) {
    // 1.54" = 200x200 px, 5 rows à size-2 (16px), step 37px
    display.setCursor(0, 15);  display.print("BOOT CONFIG");
    display.setCursor(0, 52);  display.print("FW: "); display.print(shortVer);
    display.setCursor(0, 89);  display.print(modeStr);
    display.setCursor(0, 126); display.print("Currency: "); display.print(currencyATM);
    display.setCursor(0, 163); display.print(shortURL);
  }
  else {
    // 2.13" = 250x122 px, 5 rows à size-2 (16px), step 20px
    display.setCursor(0, 16);  display.print("BOOT CONFIG");
    display.setCursor(0, 36);  display.print("FW: "); display.print(shortVer);
    display.setCursor(0, 56);  display.print(modeStr);
    display.setCursor(0, 76);  display.print("Currency: "); display.print(currencyATM);
    display.setCursor(0, 96);  display.print(shortURL);
  }

  display.nextPage();
  display.hibernate();
  delay(5000);
}

void setup()
{
  // *** for Waveshare ESP32 Driver board: HSPI must start BEFORE display.init() *** //
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
  hspi.begin(13, 12, 14, 15); // remap hspi for EPD (must be before display.init)
#endif
  initialize_display(); // connection to the e-ink display
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#endif
  // *** end Waveshare ESP32 Driver board *** //
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] Offline-LightningATM-esp32 firmware " VERSION);
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
  digitalWrite(LED_BUTTON_PIN, HIGH);                       // LED on during boot

  // ── SPIFFS: read stored config ────────────────────────────────────────────
  if (!SPIFFS.begin(true)) {
    Serial.println("[SETUP] SPIFFS mount failed");
  }
  else {
    String configJson = readFileContent("/config.json");
    if (configJson.length() > 0)
      storedDeviceString = extractDeviceStringFromJson(configJson);
  }

  // Fall back to hardcoded string if SPIFFS is empty
  if (storedDeviceString.length() == 0 && lnurlDeviceString.length() > 0)
    storedDeviceString = lnurlDeviceString;

  // ── Detect BOOT-button hold at power-on (> 5 s) → force config mode ───────
  bool forceConfigMode = false;
  if (digitalRead(BUTTON_PIN) == LOW) {
    digitalWrite(LED_BUTTON_PIN, LOW);                      // LED off while measuring
    unsigned long holdStart = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      if (millis() - holdStart > 5000) { forceConfigMode = true; break; }
      delay(50);
    }
    // wait for release
    while (digitalRead(BUTTON_PIN) == LOW) delay(50);
    digitalWrite(LED_BUTTON_PIN, HIGH);
  }

  // ── Enter config mode if unconfigured or forced ───────────────────────────
  if (storedDeviceString.length() == 0 || forceConfigMode) {
    home_screen();
    configMode(); // blocks until exit; reloads storedDeviceString internally
    if (storedDeviceString.length() == 0)
      ESP.restart(); // still not configured → restart
  }

  // ── Normal startup ────────────────────────────────────────────────────────
  attachInterrupt(BUTTON_PIN, button_pressed_itr, FALLING); // interrupt, will set button_pressed to true when button is pressed
  digitalWrite(LED_BUTTON_PIN, HIGH);                       // light up the led
  baseURLATM = getValue(storedDeviceString, ',', 0);       // setup wallet data from string
  secretATM = getValue(storedDeviceString, ',', 1);
  currencyATM = getValue(storedDeviceString, ',', 2);
  fossaMode = (storedDeviceString.indexOf("/lnurldevice/") < 0); // AES (fossa-compatible) unless explicitly lnurldevice path

  Serial.println(F("[SETUP] Device string parsed:"));
  Serial.println("  baseURL : " + baseURLATM);
  Serial.println("  secret  : " + (secretATM.length() > 6 ? secretATM.substring(0, 4) + "..." + secretATM.substring(secretATM.length() - 2) : secretATM));
  Serial.println("  currency: " + currencyATM);
  Serial.println(String("  mode    : ") + (fossaMode ? "fossa/AES (no /lnurldevice/ in URL)" : "lnurldevice/XOR"));

  boot_info_screen(); // show config summary on display for ~5 s before normal home screen
  home_screen();
}

void loop()
{
  unsigned int pulses = 0;
  unsigned long long time_last_press;

  pulses = detect_coin(); // detect_coin() is a loop to detect the input of coins, will return the amount of pulses
  if (pulses >= 2 && pulses <= 9)
  {
    digitalWrite(MOSFET_PIN, HIGH);
    digitalWrite(LED_BUTTON_PIN, LOW);
    inserted_cents += COINS[pulses];
    Serial.println("[COIN] Coin detected: +" + String(COINS[pulses]) + " cents → total: " + get_amount_string(inserted_cents));
    show_inserted_amount(inserted_cents);
  }
  else if (button_pressed && inserted_cents > 0)
  {
    digitalWrite(MOSFET_PIN, HIGH);
    button_pressed = false;
    Serial.println("[BUTTON] Button pressed → generating QR for " + get_amount_string(inserted_cents) + " (" + String(inserted_cents) + " cents)");
    char* lnurl = makeLNURL(inserted_cents);
    if (lnurl != nullptr && strlen(lnurl) > 0) {
      Serial.println("[QR] LNURL generated (" + String(strlen(lnurl)) + " chars), showing QR code...");
    }
    else {
      Serial.println(F("[QR] ERROR: LNURL generation failed!"));
    }
    qr_withdrawl_screen(lnurl);
    free(lnurl);
    Serial.println(F("[QR] QR code on display - waiting for user to scan..."));
    wait_for_user_to_scan();
    Serial.println(F("[ATM] Scan complete / timeout → returning to home screen"));
    digitalWrite(LED_BUTTON_PIN, HIGH);
    home_screen();
    digitalWrite(MOSFET_PIN, LOW);
    inserted_cents = 0;
  }
  else if (button_pressed && !pulses && !inserted_cents) // short press → double-action config, or press counter for clean screen
  {
    button_pressed = false;

    // Wait for full release of the first (short) press
    while (digitalRead(BUTTON_PIN) == LOW) delay(20);

    // ── Visual feedback: 3 quick LED blinks → "prime received, hold now" ──
    for (int i = 0; i < 6; i++) {
      digitalWrite(LED_BUTTON_PIN, i % 2 == 0 ? LOW : HIGH);
      delay(80);
    }
    digitalWrite(LED_BUTTON_PIN, HIGH);

    // ── Wait up to 2 s for a second button press ───────────────────────────
    bool gotSecondPress = false;
    unsigned long waitStart = millis();
    while (millis() - waitStart < 2000) {
      if (digitalRead(BUTTON_PIN) == LOW) { gotSecondPress = true; break; }
      delay(20);
    }

    if (gotSecondPress) {
      // Measure how long the second press is held
      unsigned long holdStart = millis();
      bool isLongPress = false;
      while (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - holdStart > 5000) { isLongPress = true; break; }
        // Rapid LED flicker while holding to show progress
        digitalWrite(LED_BUTTON_PIN, (millis() / 120) % 2);
        delay(20);
      }
      digitalWrite(LED_BUTTON_PIN, HIGH);
      while (digitalRead(BUTTON_PIN) == LOW) delay(20); // full release

      if (isLongPress) {
        Serial.println(F("[BUTTON] Double-action confirmed → entering config mode"));
        detachInterrupt(BUTTON_PIN);
        configMode();
        // Reload operative strings after potential config update
        baseURLATM = getValue(storedDeviceString, ',', 0);
        secretATM = getValue(storedDeviceString, ',', 1);
        currencyATM = getValue(storedDeviceString, ',', 2);
        fossaMode = (storedDeviceString.indexOf("/lnurldevice/") < 0);
        attachInterrupt(BUTTON_PIN, button_pressed_itr, FALLING);
        home_screen();
        return;
      }
      // Second press was short → count it toward clean-screen counter below
      button_pressed = false;
    }

    // ── Short press(es): press-counter for screen clean (storage mode) ───────
    // First press already registered; start counter at 1.
    int press_counter = 1;
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

// ─────────────────────────────────────────────────────────────────────────────
// Serial Config Mode
// Entered automatically when no device string is stored, or when BOOT button
// is held > 5 s during normal operation.
// LED blinks slowly (1 Hz) while in config mode.
// Short button press exits config mode (if device string is stored).
// Accepts web-installer serial commands: /hello /file-read /file-append
//   /file-remove /config-soft-reset
// ─────────────────────────────────────────────────────────────────────────────
void configMode()
{
  // Block coin acceptor during config mode
  digitalWrite(MOSFET_PIN, HIGH);
  Serial.println(F("[CONFIG] Coin acceptor locked (MOSFET HIGH)"));

  // ── Show config mode screen on e-paper ───────────────────────────────────
  {
    bool is27 = (display_type == "GxEPD2_270" || display_type == "GxEPD2_270_GDEY027T91");
    bool is154 = (display_type == "GxEPD2_150_BN");
    display.setRotation(1);
    display.setFullWindow();
    display.firstPage();
    display.setTextColor(GxEPD_BLACK, GxEPD_WHITE);
    // 2 lines ohne Leerzeile: gap = 1 Zeilenhöhe
    // 2.7":  264x176 → y=78, y=98  (gap 20px)
    // 2.13": 250x122 → y=44, y=64  (gap 20px)
    // 1.54": 200x200 → size-1 y=92, y=108 (gap 16px)
    if (is154) {
      display.setTextSize(1);
      display.setCursor(0, 92);  display.print("ATM ready for config");
      display.setCursor(0, 108); display.print("Use web installer..");
    }
    else {
      display.setTextSize(2);
      display.setCursor(0, is27 ? 78 : 44);
      display.print("ATM ready for config");
      display.setCursor(0, is27 ? 98 : 64);
      display.print("Use web installer..");
    }
    display.nextPage();
    display.hibernate();
  }

  Serial.flush();
  delay(300);
  while (Serial.available()) Serial.read(); // drain stale bytes

  Serial.println(F("\n==================================="));
  Serial.println(F("  Serial Config Mode Active"));
  Serial.println(F("==================================="));
  Serial.println(F("[CONFIG_MODE_ENTER]"));
  Serial.println(F("Waiting for commands..."));
  Serial.flush();
  // Repeat marker so web installer reliably detects it
  for (int i = 0; i < 3; i++) { delay(150); Serial.println(F("[CONFIG_MODE_ENTER]")); Serial.flush(); }
  while (Serial.available()) Serial.read();

  bool hasData = (storedDeviceString.length() > 0);
  unsigned long lastActivity = millis();
  const unsigned long INACTIVITY_MS = 180000UL; // 3 minutes
  static unsigned long lastFileReadTime = 0;

  // LED blink state
  unsigned long lastBlinkTime = millis();
  bool blinkState = true;
  digitalWrite(LED_BUTTON_PIN, HIGH);

  // Button exit tracking (short press)
  bool btnPrev = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long btnPressStart = 0;
  const unsigned long CONFIG_EXIT_GUARD_MS = 2000; // guard: ignore first 2 s
  unsigned long configStartTime = millis();

  while (true)
  {
    yield();

    // ── Slow LED blink (500 ms on / 500 ms off = 1 Hz) ───────────────────
    if (millis() - lastBlinkTime >= 500) {
      blinkState = !blinkState;
      digitalWrite(LED_BUTTON_PIN, blinkState ? HIGH : LOW);
      lastBlinkTime = millis();
    }

    // ── Button short-press → exit config mode (only if device string set) ─
    if (hasData && (millis() - configStartTime) > CONFIG_EXIT_GUARD_MS) {
      bool btnNow = (digitalRead(BUTTON_PIN) == LOW);
      if (btnNow && !btnPrev) btnPressStart = millis();                 // press start
      if (!btnNow && btnPrev && (millis() - btnPressStart) < 3000) {   // released < 3 s
        Serial.println(F("[CONFIG_MODE_EXIT]")); Serial.flush();
        delay(300);
        break;
      }
      btnPrev = btnNow;
    }

    // ── 180 s inactivity timeout (only if device string already available) ─
    if (hasData && (millis() - lastActivity > INACTIVITY_MS)) {
      Serial.println(F("\n--- Inactivity timeout (180 s) ---"));
      Serial.println(F("[CONFIG_MODE_EXIT]")); Serial.flush();
      delay(300);
      break;
    }

    if (Serial.available() == 0) { delay(10); continue; }
    lastActivity = millis();

    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.length() == 0) continue;

    int spacePos = data.indexOf(' ');
    String cmd = (spacePos >= 0) ? data.substring(0, spacePos) : data;
    String arg = (spacePos >= 0) ? data.substring(spacePos + 1) : "";

    // Echo (skip for /file-read to keep output clean)
    if (cmd != "/file-read") {
      if (data.length() > 80)
        Serial.println("received: " + cmd + " [" + String(data.length()) + " bytes]");
      else
        Serial.println("received: " + data);
      Serial.flush();
    }

    // ── Commands ───────────────────────────────────────────────────────────
    if (cmd == "/hello") {
      // ASCII art: ATM
      Serial.println(F("  ___  _____  __  ___"));
      Serial.println(F(" / _ |/_  _/  / |/ /"));
      Serial.println(F("/ __ | / /   /    /"));
      Serial.println(F("/_/ |_|/_/  /_/|_/"));
      Serial.println(F("[CONFIG_MODE_ENTER]"));
      Serial.flush();

    }
    else if (cmd == "/file-read") {
      if (millis() - lastFileReadTime < 1000) continue; // debounce
      lastFileReadTime = millis();
      String content = readFileContent("/config.json");
      if (content.length() > 0) {
        String response = "/file-read " + content;
        serialWriteChunked(response);
        Serial.println(); Serial.flush(); delay(50);
      }
      else {
        Serial.println(F("- Failed to open file for reading")); Serial.flush();
      }

    }
    else if (cmd == "/file-remove") {
      SPIFFS.remove("/config.json");
      Serial.println(F("- Remove done")); Serial.flush();

    }
    else if (cmd == "/file-append") {
      // arg = "config.json {...json...}"
      int fileNameEnd = arg.indexOf(' ');
      String fileData = (fileNameEnd >= 0) ? arg.substring(fileNameEnd + 1) : "";
      File f = SPIFFS.open("/config.json", FILE_APPEND);
      if (!f) f = SPIFFS.open("/config.json", FILE_WRITE);
      if (f) { f.println(fileData); f.close(); }
      Serial.println(F("- Append done")); Serial.flush();

    }
    else if (cmd == "/config-soft-reset" || cmd == "/config-done") {
      Serial.println(F("[CONFIG_MODE_EXIT]")); Serial.flush();
      delay(300);
      break;

    }
    else {
      Serial.println(F("- Unknown command")); Serial.flush();
    }
  }

  // ── Exiting config mode ──────────────────────────────────────────────────
  digitalWrite(MOSFET_PIN, LOW); // Re-enable coin acceptor
  Serial.println(F("[CONFIG] Coin acceptor unlocked (MOSFET LOW)"));
  digitalWrite(LED_BUTTON_PIN, HIGH); // LED solid on
  button_pressed = false;

  // Re-read config so storedDeviceString is up to date
  String configJson = readFileContent("/config.json");
  if (configJson.length() > 0)
    storedDeviceString = extractDeviceStringFromJson(configJson);
}

// blocking loop which is called when the qr code is shown
void wait_for_user_to_scan()
{
  unsigned long long time;
  bool light_on;
  unsigned long lastStatusMs = 0;

  if (DEBUG_MODE)
    Serial.println("Waiting for user to scan qr code and press button...");
  light_on = true;
  time = millis();  // save start time
  digitalWrite(LED_BUTTON_PIN, HIGH);  // light up the led
  delay(5000);
  button_pressed = false;
  Serial.println(F("[QR] Scan QR or press button - auto-return in 10 min"));
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
    // Print status every 10 s with remaining time
    unsigned long now = millis();
    if (now - lastStatusMs >= 10000) {
      lastStatusMs = now;
      unsigned long elapsed = (now - time) / 1000;
      unsigned long remaining = (elapsed < 600) ? (600 - elapsed) : 0;
      Serial.println("[QR] Still showing QR - " + String(remaining) + "s remaining, press button to confirm");
    }
    delay(500);
  }
  Serial.println(F("[QR] Exit waiting"));
}

unsigned int detect_coin()
{
  unsigned long last_pulse;
  unsigned int pulses;
  bool prev_value;
  bool read_value;
  unsigned long entering_time;
  unsigned long current_time;
  static unsigned long lastAliveMs = 0; // heartbeat timer survives between calls

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
        digitalWrite(MOSFET_PIN, HIGH);
      }
    }
    prev_value = read_value;
    current_time = millis();
    if (pulses > 0 && (current_time - last_pulse > PULSE_TIMEOUT))
    {
      break;
    }
    else if (pulses == 0 && inserted_cents == 0 && (current_time - lastAliveMs >= 5000))
    {
      lastAliveMs = current_time;
      Serial.println(F("[ALIVE] Waiting for coins..."));
    }
    else if (pulses == 0 && inserted_cents > 0 && (current_time - lastAliveMs >= 5000))
    {
      lastAliveMs = current_time;
      Serial.println("[COIN] Total: " + String(inserted_cents) + " cents - press button for QR");
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
    display.init(115200, true, 10, false); // reset_duration=10ms for GDEM0213B74/SSD1680
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

////////////////////////////////////////////
///////////////LNURL STUFF//////////////////
////USING STEPAN SNIGREVS GREAT CRYTPO//////
////////////THANK YOU STEPAN////////////////
////////////////////////////////////////////

int xor_encrypt(uint8_t* output, size_t outlen, uint8_t* key, size_t keylen, uint8_t* nonce, size_t nonce_len, uint64_t pin, uint64_t amount_in_cents)
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

// AES-256-CBC encryption compatible with LNbits AESCipher (OpenSSL/CryptoJS "Salted__" format).
// The server (fossa extension) decrypts with AESCipher.decrypt(payload, urlsafe=True).
// Plaintext format: "pin:amount_cents" (e.g. "1234:150" for a 1.50 EUR voucher)
// Key derivation: EVP_BytesToKey with MD5 (same as OpenSSL -k flag / CryptoJS default)
// Returns base64url-encoded "Salted__" | salt(8) | ciphertext
// Length is always a multiple of 22 base64 chars (= multiple of 16 raw bytes) as the server checks.
String aes_encrypt_fossa(const char* key, size_t key_len, int pin, int amount_cents)
{
  // 1. Random 8-byte salt
  uint8_t salt[8];
  for (int i = 0; i < 8; i++)
    salt[i] = (uint8_t)random(256);

  // 2. EVP_BytesToKey: digest chain over (key || salt) until 48 bytes (32 key + 16 IV)
  //    d0 = MD5(key || salt)
  //    d1 = MD5(d0 || key || salt)
  //    d2 = MD5(d1 || key || salt)
  //    aes_key = d0 || d1 (first 32 bytes)
  //    aes_iv  = d2        (bytes 32-47)
  uint8_t key_data[256];
  // Ensure we never write past key_data[256] or tmp[16 + 256]:
  // key_data holds (effective_key_len + 8) bytes; tmp holds (16 + kd_len) bytes.
  const size_t max_key_len = 256 - 8; // 248 bytes
  size_t       effective_key_len = key_len > max_key_len ? max_key_len : key_len;
  size_t       kd_len = effective_key_len + 8;
  memcpy(key_data, key, effective_key_len);
  memcpy(key_data + effective_key_len, salt, 8);

  // helper lambda to compute MD5 via Arduino MD5Builder (avoids mbedTLS version issues)
  auto md5_once = [](const uint8_t* buf, size_t len, uint8_t out[16]) {
    MD5Builder md5;
    md5.begin();
    md5.add(const_cast<uint8_t*>(buf), (uint16_t)len);
    md5.calculate();
    md5.getBytes(out);
    };

  uint8_t d[48];
  uint8_t tmp[16 + 256]; // scratch for d_prev || key_data

  md5_once(key_data, kd_len, d);                 // d0

  memcpy(tmp, d, 16);                // d1 = MD5(d0 || key_data)
  memcpy(tmp + 16, key_data, kd_len);
  md5_once(tmp, 16 + kd_len, d + 16);

  memcpy(tmp, d + 16, 16);                // d2 = MD5(d1 || key_data)
  memcpy(tmp + 16, key_data, kd_len);
  md5_once(tmp, 16 + kd_len, d + 32);

  uint8_t* aes_key = d;       // bytes  0-31
  uint8_t  aes_iv[16];
  memcpy(aes_iv, d + 32, 16); // bytes 32-47

  // 3. Plaintext: "pin:amount_cents"
  char    plaintext[33];
  int     pt_len = snprintf(plaintext, sizeof(plaintext), "%d:%d", pin, amount_cents);
  // Guard: snprintf truncated, or padded block would exceed our fixed buffers (max 16 bytes padded)
  if (pt_len < 0 || pt_len >= (int)sizeof(plaintext) || pt_len > 15) {
#ifdef DEBUG_MODE
    Serial.println(F("aes_encrypt_fossa: plaintext too long"));
#endif
    return String();
  }

  // 4. PKCS7 padding to next multiple of 16 (always at least 1 byte of padding)
  //    pt_len <= 15 => padded_len == 16, fits into padded[16] / ciphertext[16] / salted[32]
  int     padded_len = 16;
  uint8_t padded[16];
  memset(padded, 0, sizeof(padded));
  memcpy(padded, (const uint8_t*)plaintext, pt_len);
  uint8_t pad_byte = (uint8_t)(padded_len - pt_len);
  for (int i = pt_len; i < padded_len; i++)
    padded[i] = pad_byte;

  // 5. AES-256-CBC encrypt
  uint8_t ciphertext[16]; // padded_len is always 16
  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);
  int ret = mbedtls_aes_setkey_enc(&aes_ctx, aes_key, 256);
  if (ret != 0) {
#ifdef DEBUG_MODE
    Serial.print(F("AES setkey_enc failed: "));
    Serial.println(ret);
#endif
    mbedtls_aes_free(&aes_ctx);
    return String();
  }

  ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, padded_len, aes_iv, padded, ciphertext);
  if (ret != 0) {
#ifdef DEBUG_MODE
    Serial.print(F("AES crypt_cbc failed: "));
    Serial.println(ret);
#endif
    mbedtls_aes_free(&aes_ctx);
    return String();
  }
  mbedtls_aes_free(&aes_ctx);

  // 6. Assemble: "Salted__" (8) | salt (8) | ciphertext (padded_len)
  uint8_t salted[16 + 16]; // 8 header + 8 salt + 16 ciphertext
  memcpy(salted, "Salted__", 8);
  memcpy(salted + 8, salt, 8);
  memcpy(salted + 16, ciphertext, padded_len);
  int total_len = 16 + padded_len; // 32 bytes for typical amounts → 44 base64 chars (44 % 22 == 0 ✓)

  // 7. Base64url WITH padding (required: server checks len(payload) % 22 == 0)
  return toBase64(salted, total_len, BASE64_URLSAFE);
}

char* makeLNURL(float total)
{
  int randomPin = random(1000, 9999);
  // fossa/AES: server expects only ?p=  (no atm=1)
  // lnurldevice/XOR: server expects ?atm=1&p=
  String preparedURL = baseURLATM + (fossaMode ? "?p=" : "?atm=1&p=");

  if (fossaMode)
  {
    // fossa extension: AES-256-CBC (OpenSSL "Salted__" format, plaintext "pin:amount_cents")
    preparedURL += aes_encrypt_fossa(secretATM.c_str(), secretATM.length(), randomPin, (int)lroundf(total));
  }
  else
  {
    // lnurldevice extension: XOR-HMAC encryption (legacy)
    byte nonce[8];
    for (int i = 0; i < 8; i++)
    {
      nonce[i] = random(256);
    }
    byte payload[51]; // 51 bytes is max one can get with xor-encryption
    size_t payload_len = xor_encrypt(payload, sizeof(payload), (uint8_t*)secretATM.c_str(), secretATM.length(), nonce, sizeof(nonce), randomPin, float(total));
    preparedURL += toBase64(payload, payload_len, BASE64_URLSAFE | BASE64_NOPADDING);
  }

  if (DEBUG_MODE)
    Serial.println(preparedURL);
  char Buf[200];
  preparedURL.toCharArray(Buf, 200);
  char* url = Buf;
  byte* data = (byte*)calloc(strlen(url) * 2, sizeof(byte));
  if (!data)
    return (NULL);
  size_t len = 0;
  int res = convert_bits(data, &len, 5, (byte*)url, strlen(url), 8, 1);
  char* charLnurl = (char*)calloc(strlen(url) * 2, sizeof(byte));
  if (!charLnurl)
  {
    free(data);
    return (NULL);
  }
  bech32_encode(charLnurl, "lnurl", data, len);
  to_upper(charLnurl);
  free(data);
  return (charLnurl);
}

void to_upper(char* arr)
{
  for (size_t i = 0; i < strlen(arr); i++)
  {
    if (arr[i] >= 'a' && arr[i] <= 'z')
    {
      arr[i] = arr[i] - 'a' + 'A';
    }
  }
}

// Function to seperate the LNURLDevice string in key, url and currency
String getValue(const String data, char separator, int index)
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