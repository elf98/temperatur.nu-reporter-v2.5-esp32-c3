// TRDisplay.cpp
// OLED display handler for ESP32-C3 0.42" 72x40 SSD1306
#include "TRDisplay.h"
#include <Arduino.h>

#define SCREEN_W 72
#define SCREEN_H 40

TRDisplay::TRDisplay()
  : u8g2(U8G2_R0, /* reset */ U8X8_PIN_NONE, /* SCL */ 6, /* SDA */ 5)
{
}

void TRDisplay::begin() {
  Wire.begin(5, 6);
  u8g2.begin();
}

void TRDisplay::bootScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(0, 14, "Temp.nu");
  u8g2.drawStr(0, 30, "Reporter");
  u8g2.sendBuffer();
}

void TRDisplay::connectingScreen(const String& text) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(0, 14, "Ansluter");
  u8g2.setFont(u8g2_font_6x13_tf);
  String display_text = text.substring(0, 11);
  u8g2.drawStr(0, 30, display_text.c_str());
  u8g2.sendBuffer();
}

void TRDisplay::APScreen(const String& ssid) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(0, 14, "WiFi AP:");
  u8g2.setFont(u8g2_font_6x13_tf);
  String display_ssid = ssid.substring(0, 11);
  u8g2.drawStr(0, 30, display_ssid.c_str());
  u8g2.sendBuffer();
}

// Sensortest i AP-läget (knapptryck när portalen är aktiv vid uppstart).
// Till skillnad från screenSensors visas S2 alltid, så en saknad andra sensor syns.
void TRDisplay::sensorTestScreen(const float* temperatures, int sensorCount) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_8x13B_tf);

  for (int i = 0; i < 2; i++) {
    char line[16];
    float t = temperatures[i];
    if (i < sensorCount && t != -127.0f && t != 85.0f)
      snprintf(line, sizeof(line), "S%d: %.1f", i + 1, t);
    else
      snprintf(line, sizeof(line), "S%d: ---", i + 1);
    u8g2.drawStr(0, i == 0 ? 16 : 33, line);
  }

  u8g2.sendBuffer();
}

void TRDisplay::printDot() {
  static int dotX = 0;
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(dotX, SCREEN_H, ".");
  u8g2.sendBuffer();
  dotX += 6;
  if (dotX > SCREEN_W - 6) dotX = 0;
}

void TRDisplay::mainScreen(int signalStrength, const float* temperatures,
                           int sensorCount, uint32_t pin,
                           const String& ssid, const String& ip, int rssiPercent,
                           bool uploadOk, const char* fwVersion,
                           const String& localTime) {
  _signalStrength = signalStrength;
  _temperatures = temperatures;
  _sensorCount = sensorCount;
  _pin = pin;
  _ssid = ssid;
  _ip = ip;
  _rssiPercent = rssiPercent;
  _uploadOk = uploadOk;
  _fwVersion = fwVersion;
  _localTime = localTime;

  if (_sensorCount >= 1) {
    _lowestTemperature = (_sensorCount == 2) ? min(_temperatures[0], _temperatures[1]) : _temperatures[0];
  } else {
    _lowestTemperature = -127.0f;
  }

  switch (_screenMode) {
    case SCREEN_TEMPERATURE: screenTemperature(); break;
    case SCREEN_SENSORS:     screenSensors();     break;
    case SCREEN_PIN:         screenPin();         break;
    case SCREEN_WIFI:        screenWifi();        break;
    default:                 screenTemperature(); break;
  }
}

// Skärm 1: Temperatur - stor visning
void TRDisplay::screenTemperature() {
  u8g2.clearBuffer();

  if (_sensorCount == 0 || _lowestTemperature == -127.0f || _lowestTemperature == 85.0f) {
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(0, 16, "Ingen");
    u8g2.drawStr(0, 34, "sensor");
    u8g2.sendBuffer();
    return;
  }

  char tempStr[10];
  snprintf(tempStr, sizeof(tempStr), "%.1f", _lowestTemperature);

  u8g2.setFont(u8g2_font_logisoso26_tn);
  int tw = u8g2.getStrWidth(tempStr);
  u8g2.drawStr((SCREEN_W - tw) / 2, 34, tempStr);

  if (!_uploadOk) {
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(SCREEN_W - 6, 40, "!");
  }

  u8g2.sendBuffer();
}

// Skärm 2: PIN - stor och läsbar
void TRDisplay::screenPin() {
  u8g2.clearBuffer();

  // Formatera PIN med mellanrum: 1234567 -> "1 234 567"
  char raw[12];
  snprintf(raw, sizeof(raw), "%lu", (unsigned long)_pin);
  int len = strlen(raw);

  char pinStr[16];
  int j = 0;
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0)
      pinStr[j++] = ' ';
    pinStr[j++] = raw[i];
  }
  pinStr[j] = '\0';

  int slen = strlen(pinStr);

  // Dela upp vid första mellanslaget
  int cut = -1;
  for (int i = 0; i < slen; i++) {
    if (pinStr[i] == ' ') { cut = i; break; }
  }

  char topDigits[16] = "";
  char botStr[16] = "";

  if (cut > 0) {
    strncpy(topDigits, pinStr, cut);
    topDigits[cut] = '\0';
    strcpy(botStr, &pinStr[cut + 1]);
  } else {
    strcpy(topDigits, pinStr);
  }

  u8g2.setFont(u8g2_font_9x15B_tf);

  if (len < 7) {
    // Kort PIN: "PIN" på rad 1, alla siffror på rad 2 högerjusterat
    u8g2.drawStr(0, 15, "PIN");
    int pw = u8g2.getStrWidth(pinStr);
    u8g2.drawStr(SCREEN_W - pw, 37, pinStr);
  } else {
    // Lång PIN: "PIN" + första siffror på rad 1, resten på rad 2
    u8g2.drawStr(0, 15, "PIN");
    int tw = u8g2.getStrWidth(topDigits);
    u8g2.drawStr(SCREEN_W - tw, 15, topDigits);

    if (botStr[0] != '\0') {
      int pw = u8g2.getStrWidth(botStr);
      u8g2.drawStr(SCREEN_W - pw, 37, botStr);
    }
  }

  u8g2.sendBuffer();
}

// Skärm 3: Sensorinfo
void TRDisplay::screenSensors() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_8x13B_tf);

  if (_sensorCount == 0) {
    u8g2.drawStr(0, 16, "Ingen");
    u8g2.drawStr(0, 33, "sensor");
    u8g2.sendBuffer();
    return;
  }

  char t1Str[16];
  if (_temperatures[0] != -127.0f && _temperatures[0] != 85.0f)
    snprintf(t1Str, sizeof(t1Str), "S1: %.1f", _temperatures[0]);
  else
    snprintf(t1Str, sizeof(t1Str), "S1: ---");
  u8g2.drawStr(0, 16, t1Str);

  if (_sensorCount >= 2) {
    char t2Str[16];
    if (_temperatures[1] != -127.0f && _temperatures[1] != 85.0f)
      snprintf(t2Str, sizeof(t2Str), "S2: %.1f", _temperatures[1]);
    else
      snprintf(t2Str, sizeof(t2Str), "S2: ---");
    u8g2.drawStr(0, 33, t2Str);
  }

  u8g2.sendBuffer();
}

// Skärm 4: WiFi - signalstaplar + FW-version + klocka
void TRDisplay::screenWifi() {
  u8g2.clearBuffer();

  // Tid (vänster) — regular weight, 7x13 så den blir läsbar.
  // Version får hålla emphasis via fet stil.
  u8g2.setFont(u8g2_font_7x13_tf);
  if (_localTime.length() > 0) {
    u8g2.drawStr(0, 13, _localTime.c_str());
  } else {
    u8g2.drawStr(0, 13, "WiFi");
  }

  // Version fet (höger) — primärinfo. Bottom-aligned med klockan.
  // Worst case: "12:34" 35px + "2.5.10" 36px = 71 av 72. 1 px gap.
  u8g2.setFont(u8g2_font_6x13B_tf);
  if (_fwVersion && _fwVersion[0] != '\0') {
    int vw = u8g2.getStrWidth(_fwVersion);
    u8g2.drawStr(SCREEN_W - vw, 13, _fwVersion);
  }

  // Signalstaplar nedtill
  int barW = 9;
  int gap = 4;
  int totalW = 4 * barW + 3 * gap;
  int startX = (SCREEN_W - totalW) / 2;

  for (int i = 0; i < 4; i++) {
    int barH = 4 + 4 * i; // 4, 8, 12, 16
    int x = startX + i * (barW + gap);
    int y = 40 - barH;
    if (i < _signalStrength)
      u8g2.drawBox(x, y, barW, barH);
    else
      u8g2.drawFrame(x, y, barW, barH);
  }

  u8g2.sendBuffer();
}

// Visas live medan användaren håller in knappen på WiFi-skärmen.
// Bara text — visar vad som händer om knappen släpps NU.
// Stages: OTA -> Portal -> AVBRYT (säker exit) -> RESET
void TRDisplay::showLongPressIndicator(unsigned long heldMs) {
  const char* releaseAction;
  if (heldMs < 5000) {
    releaseAction = "OTA";
  } else if (heldMs < 10000) {
    releaseAction = "Portal";
  } else if (heldMs < 15000 || heldMs >= 20000) {
    releaseAction = "AVBRYT";
  } else {
    releaseAction = "RESET!";
  }

  u8g2.clearBuffer();

  // Översta raden: "Slapp =" (ASCII-substitution: ä → a för OLED-displayen)
  u8g2.setFont(u8g2_font_6x10_tf);
  const char* prefix = "Slapp =";
  int pw = u8g2.getStrWidth(prefix);
  u8g2.drawStr((SCREEN_W - pw) / 2, 12, prefix);

  // Stora raden: action
  u8g2.setFont(u8g2_font_7x14B_tf);
  int tw = u8g2.getStrWidth(releaseAction);
  u8g2.drawStr((SCREEN_W - tw) / 2, 32, releaseAction);

  u8g2.sendBuffer();
}

// Enkel statusskärm för OTA/portal/reset-händelser
void TRDisplay::statusScreen(const char* line1, const char* line2) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  // 7 px/tecken ger bara 10 tecken på 72 px — smalare font för längre rubriker
  if (u8g2.getStrWidth(line1) > u8g2.getDisplayWidth())
    u8g2.setFont(u8g2_font_6x13B_tf);
  u8g2.drawStr(0, 16, line1);
  if (line2 && line2[0] != '\0') {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 32, line2);
  }
  u8g2.sendBuffer();
}

void TRDisplay::nextScreenMode() {
  _screenMode = (ScreenMode)((_screenMode + 1) % SCREEN_MODE_COUNT);
  Serial.printf("Skarmlage: %d\n", _screenMode);
}

void TRDisplay::setScreenMode(ScreenMode mode) {
  _screenMode = mode;
  Serial.printf("Skarmlage: %d (auto)\n", _screenMode);
}

ScreenMode TRDisplay::getScreenMode() {
  return _screenMode;
}
