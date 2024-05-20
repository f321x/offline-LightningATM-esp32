#include "Display.hpp"

Display::Display(const String& display, const String& board, const bool debug_mode) : _debugMode(debug_mode) {
    if (!_checkInput(display.c_str(), board.c_str()))
        throw std::runtime_error("Invalid display or board");
    if (display == "GxEPD2_270") {
        if (board == "Waveshare")
            _display27 = new Display27(GxEPD2_270(5, 17, 16, 4));
        else if (board == "ESP32")
            _display27 = new Display27(GxEPD2_270(26, 25, 33, 27));
        _display27->init(115200, true, 2, false);
    }
    else if (display == "GxEPD2_270_GDEY027T91") {
        if (board == "Waveshare")
            _display27_v2 = new Display27v2(GxEPD2_270_GDEY027T91(5, 17, 16, 4));
        else if (board == "ESP32")
            _display27_v2 = new Display27v2(GxEPD2_270_GDEY027T91(26, 25, 33, 27));
        _display27->init(115200, true, 2, false);
    }
    else if (display == "GxEPD2_213_B74") {
        if (board == "Waveshare")
            _display213v3 = new Display213v3(GxEPD2_213_B74(5, 17, 16, 4));
        else if (board == "ESP32")
            _display213v3 = new Display213v3(GxEPD2_213_B74(26, 25, 33, 27));
        _display213v3->init(115200, true, 2, false);
    }
    else if (display == "GxEPD2_213_flex") {
        if (board == "Waveshare")
            _display213flex = new Display213flex(GxEPD2_213_flex(5, 17, 16, 4));
        else if (board == "ESP32")
            _display213flex = new Display213flex(GxEPD2_213_flex(26, 25, 33, 27));
        _display213flex->init(115200, true, 2, false);
    }
    else if (display == "GxEPD2_150_BN") {
        if (board == "Waveshare")
            _display154 = new Display154(GxEPD2_150_BN(5, 17, 16, 4));
        else if (board == "ESP32")
            _display154 = new Display154(GxEPD2_150_BN(26, 25, 33, 27));
        _display154->init(115200, true, 2, false);
    }
}

Display::~Display() {
    // delete display;
}

bool    Display::_checkInput(const std::string display, const std::string board) const {
    if (std::find(_supportedDisplays.begin(), _supportedDisplays.end(), display) == _supportedDisplays.end()) {
        return false;
    }
    if (std::find(_supportedBoards.begin(), _supportedBoards.end(), board) == _supportedBoards.end()) {
        return false;
    }
    return true;
}

