// TRDisplay.h
// OLED display handler for ESP32-C3 0.42" 72x40 SSD1306
#ifndef TRDISPLAY_H
#define TRDISPLAY_H

#include <U8g2lib.h>
#include <Wire.h>

// Screen modes - button cycles: Temp → Sensor → PIN → WiFi
enum ScreenMode {
  SCREEN_TEMPERATURE,
  SCREEN_SENSORS,
  SCREEN_PIN,
  SCREEN_WIFI,
  SCREEN_MODE_COUNT
};

class TRDisplay {
  public:
    TRDisplay();
    void begin();

    void bootScreen();
    void connectingScreen(const String& text);
    void APScreen(const String& ssid);
    void mainScreen(int signalStrength, const float* temperatures,
                    int sensorCount, uint32_t pin,
                    const String& ssid, const String& ip, int rssiPercent,
                    bool uploadOk, const char* fwVersion,
                    const String& localTime);

    void printDot();
    void nextScreenMode();
    void setScreenMode(ScreenMode mode);
    ScreenMode getScreenMode();

    // Långtrycks-indikator (visas live under press på WiFi-skärmen)
    void showLongPressIndicator(unsigned long heldMs);
    // Statusskärm under OTA / portal / reset
    void statusScreen(const char* line1, const char* line2 = "");

  private:
    void screenTemperature();
    void screenSensors();
    void screenPin();
    void screenWifi();

    // Cached values for display
    const float* _temperatures;
    float _lowestTemperature;
    int _signalStrength;
    int _sensorCount;
    uint32_t _pin;
    String _ssid;
    String _ip;
    int _rssiPercent;
    bool _uploadOk;
    const char* _fwVersion = "";
    String _localTime;

    ScreenMode _screenMode = SCREEN_TEMPERATURE;

    // U8g2 display object for 72x40 SSD1306 via hardware I2C
    // SDA=GPIO5, SCL=GPIO6
    U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2;
};

#endif // TRDISPLAY_H
