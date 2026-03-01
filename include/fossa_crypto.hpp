#pragma once

#include <mbedtls/aes.h>
#include <mbedtls/md5.h>
#include <Arduino.h>
#include <cstring>
#include "Bitcoin.h"
#include "util.hpp"


String makeLNURLLegacy(String secretATM, float total, String baseURLATM);
String makeLNURLFossa(String secretATM, float total, String baseURLATM);