#pragma once

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "qrcode.h"
#include <vector>
#include <algorithm>
#include <string>

#define MAX_DISPLAY_BUFFER_SIZE 65536ul  // esp32 max display buffer size
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

using Display27 = GxEPD2_BW<GxEPD2_270, MAX_HEIGHT(GxEPD2_270)>;
using Display27v2 = GxEPD2_BW<GxEPD2_270_GDEY027T91, MAX_HEIGHT(GxEPD2_270_GDEY027T91)>;
using Display213v3 = GxEPD2_BW<GxEPD2_213_B74, MAX_HEIGHT(GxEPD2_213_B74)>;
using Display213flex = GxEPD2_BW<GxEPD2_213_flex, MAX_HEIGHT(GxEPD2_213_flex)>;
using Display154 = GxEPD2_BW<GxEPD2_150_BN, MAX_HEIGHT(GxEPD2_150_BN)>;

class Display {
public:
    Display(const String& display, const String& board, const bool debug_mode);
    ~Display();
    Display(const Display& other) = delete;
    Display& operator=(const Display& other) = delete;

private:
    const bool _debugMode;
    const std::vector<std::string> _supportedDisplays = { "GxEPD2_270", "GxEPD2_270_GDEY027T91", "GxEPD2_213_B74", "GxEPD2_213_flex", "GxEPD2_150_BN" };
    const std::vector<std::string> _supportedBoards = { "Waveshare", "ESP32" };

    bool _checkInput(const std::string display, const std::string board) const;

    Display27* _display27;
    Display27v2* _display27_v2;
    Display213v3* _display213v3;
    Display213flex* _display213flex;
    Display154* _display154;


};