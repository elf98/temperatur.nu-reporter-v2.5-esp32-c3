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
  bool sendDataToWeb(float temperature, int sensorCount);
  void setTxPowerHigh();
  void setTxPowerLow();
  int getRSSIasPercentage();
  void checkWiFiStatus();
  uint32_t getChipId();
  String getMacAddress();
}

#endif // TRESP_H
