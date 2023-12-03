#include <Arduino.h>

// ########################################
// ###########    USER ACTION   ###########
// ########################################
// Generate and copy in LNbits with the LNURLDevice extension the string for the ATM and paste it here:
String lnurlDeviceString = "https://legend.lnbits.com/lnurldevice/api/v1/lnurl/idexample,keyexample,EUR";
// ########################################
// ########################################
// ########################################

String baseURLATM;
String secretATM;
String currencyATM;
String getValue(String data, char separator, int index);

void setup()
{
  baseURLATM = getValue(lnurlDeviceString, ',', 0);
  secretATM = getValue(lnurlDeviceString, ',', 1);
  currencyATM = getValue(lnurlDeviceString, ',', 2);
}

String getValue(String data, char separator, int index) // seperate string function
{
  int found = 0;
  int strIndex[] = {0, -1};
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
