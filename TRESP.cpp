// TRESP.cpp
// WiFi and HTTP handler for ESP32-C3
#include "TRESP.h"
#include <Arduino.h>
#include <esp_mac.h>
#include <esp_wifi.h>

namespace TRESP {

  // Generate a chip ID similar to ESP8266's ESP.getChipId()
  // Uses the last 3 bytes of the MAC address
  uint32_t getChipId() {
    uint32_t chipId = 0;
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    // Combine last 3 bytes like ESP8266 does
    chipId = (mac[3] << 16) | (mac[4] << 8) | mac[5];
    return chipId;
  }

  String getMacAddress() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
  }

  void printESPInfo() {
    Serial.printf("ESP CHIP ID (PIN): %lu\n", (unsigned long)getChipId());
    Serial.printf("ESP MAC: %s\n", getMacAddress().c_str());
    Serial.printf("ESP32-C3 Rev: %d\n", ESP.getChipRevision());
  }

  void setupWiFi(const char* APName, void (*apCallback)(WiFiManager*), void (*saveCallback)()) {
    WiFiManager wifiManager;

    // Set hostname
    String hostname = "Temperatur_" + String(getChipId());
    WiFi.setHostname(hostname.c_str());

    // Break after config so we can reboot on failure
    wifiManager.setBreakAfterConfig(true);

    // AP callback
    wifiManager.setAPCallback(apCallback);

    // Save callback
    wifiManager.setSaveConfigCallback(saveCallback);

    // Enable captive portal redirect
    wifiManager.setCaptivePortalEnable(true);

    // Use 8.8.8.8 as AP IP - triggers captive portal detection on Android
    wifiManager.setAPStaticIPConfig(IPAddress(8,8,8,8), IPAddress(8,8,8,8), IPAddress(255,255,255,0));

    // Svensk titel i portalen
    wifiManager.setTitle("Temperatur.nu Reporter");
    wifiManager.setDarkMode(true);

    // If a network is saved, timeout the config portal after 5 min
    wifiManager.setConfigPortalTimeout(5 * 60);

    // Sänk AP-effekt — telefonen är nära vid setup
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    // Attempt auto-connect, or start config portal
    wifiManager.autoConnect(APName);

    // If still not connected, reboot
    checkWiFiStatus();

    // Sänk WiFi-sändningseffekt mellan sändningar
    setTxPowerLow();

    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }

  bool sendDataToWeb(float temperature, int sensorCount) {
    Serial.println("--- TRESP::sendDataToWeb ---");

    setTxPowerHigh();
    delay(50); // Låt radion stabilisera sig

    checkWiFiStatus();

    static unsigned long successfulUploads = 1;

    // Build URL matching original protocol
    String url = "http://report.temperatur.nu/rapportera_v2.php?pin="
      + String(getChipId())
      + "&mac=" + getMacAddress()
      + "&t=" + String(temperature)
      + "&n=" + String(sensorCount)
      + "&u=" + String(successfulUploads);

    Serial.println(url);

    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    bool success = false;
    if (httpCode > 0) {
      Serial.printf("HTTP %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);
        successfulUploads++;
        success = true;
      }
    } else {
      Serial.printf("HTTP error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();

    setTxPowerLow();

    return success;
  }

  void setTxPowerHigh() {
    esp_wifi_set_max_tx_power(78); // 19.5dBm (nära max) för sändning
    Serial.println("TX power: high");
  }

  void setTxPowerLow() {
    esp_wifi_set_max_tx_power(32); // 8dBm mellan sändningar
    Serial.println("TX power: low");
  }

  int getRSSIasPercentage() {
    int rssi = WiFi.RSSI();
    if (rssi <= -100) return 0;
    if (rssi >= -50) return 100;
    return 2 * (rssi + 100);
  }

  void checkWiFiStatus() {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, rebooting...");
      ESP.restart();
      delay(1000);
    }
  }

} // namespace TRESP
