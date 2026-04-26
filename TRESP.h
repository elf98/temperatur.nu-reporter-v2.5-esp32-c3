// TRESP.h
// WiFi and HTTP handler for ESP32-C3
#ifndef TRESP_H
#define TRESP_H

#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>

namespace TRESP {
  void printESPInfo();
  void setupWiFi(const char* APName, void (*apCallback)(WiFiManager*), void (*saveCallback)());
  // Sätt FW-version som inkluderas i upload-URL:en. Anropas en gång i setup().
  void setFwVersion(const char* fwVersion);
  // temperature = lägsta (det temperatur.nu sparar).
  // t1, t2 = individuella sensorvärden, för debug. -127 om ej ansluten.
  bool sendDataToWeb(float temperature, int sensorCount, float t1, float t2);
  void setTxPowerHigh();
  void setTxPowerLow();
  int getRSSIasPercentage();
  void checkWiFiStatus();
  uint32_t getChipId();
  String getMacAddress();

  // Portal-entry (manuellt). Returnerar true om ny WiFi sparades (caller bör reboota),
  // false om avslutad via knapp eller timeout (caller återgår normalt).
  bool startConfigPortal(const char* APName,
                         void (*apCallback)(WiFiManager*),
                         void (*saveCallback)(),
                         int buttonPin,
                         unsigned long timeoutMs = 5UL * 60UL * 1000UL);
  void resetWiFiAndRestart();

  // NTP — sätter svensk tidszon. Kallas efter WiFi-anslutning.
  void setupNTP();
  // Returnerar true om vi har en giltig tid från NTP
  bool isTimeValid();
  // Returnerar lokal timme (0-23), -1 om tid ej giltig
  int getLocalHour();
  // Returnerar "HH:MM" från lokal tid, eller "--:--" om ej synkat
  String getLocalTimeString();
}

#endif // TRESP_H
