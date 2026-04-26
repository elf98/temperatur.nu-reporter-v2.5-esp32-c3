// TRESP.cpp
// WiFi and HTTP handler for ESP32-C3
#include "TRESP.h"
#include <Arduino.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <time.h>

namespace TRESP {

  static const char* g_fwVersion = "";

  void setFwVersion(const char* fwVersion) {
    g_fwVersion = fwVersion ? fwVersion : "";
  }

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

  bool sendDataToWeb(float temperature, int sensorCount, float t1, float t2) {
    Serial.println("--- TRESP::sendDataToWeb ---");

    setTxPowerHigh();
    delay(50); // Låt radion stabilisera sig

    checkWiFiStatus();

    static unsigned long successfulUploads = 1;

    // Build URL matching original protocol
    String url = "http://report.temperatur.nu/rapportera_v2.php?pin="
      + String(getChipId())
      + "&mac=" + getMacAddress()
      + "&fw=" + String(g_fwVersion)
      + "&t=" + String(temperature)
      + "&n=" + String(sensorCount)
      + "&u=" + String(successfulUploads);

    // Individuella sensorvärden för debug
    if (sensorCount >= 1) url += "&t1=" + String(t1);
    if (sensorCount >= 2) url += "&t2=" + String(t2);

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

  // --- Portal-entry (manuellt öppnad via långtryck) ---
  // Returvärde: true om portalen avslutades med ny WiFi-spara (caller bör reboota),
  //             false om timeout eller knapptryck (caller återgår till normal drift).
  bool startConfigPortal(const char* APName,
                         void (*apCallback)(WiFiManager*),
                         void (*saveCallback)(),
                         int buttonPin,
                         unsigned long timeoutMs) {
    WiFiManager wm;
    wm.setAPCallback(apCallback);
    wm.setSaveConfigCallback(saveCallback);
    wm.setTitle("Temperatur.nu Reporter");
    wm.setDarkMode(true);
    wm.setCaptivePortalEnable(true);
    wm.setAPStaticIPConfig(IPAddress(8,8,8,8), IPAddress(8,8,8,8), IPAddress(255,255,255,0));
    wm.setConfigPortalBlocking(false); // vi kör vår egen loop

    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    Serial.println("Manuellt initierad config-portal");
    // OBS: i non-blocking mode returnerar startConfigPortal() alltid false,
    // även när portalen startade ok. Ignorera returvärdet.
    wm.startConfigPortal(APName);

    bool saved = false;
    unsigned long startMs = millis();

    // Vänta på save, timeout, eller knapptryck för att avsluta
    while (millis() - startMs < timeoutMs) {
      // wm.process() returnerar true när användaren har sparat och anslutit
      if (wm.process()) {
        saved = true;
        break;
      }

      // Knapptryck = avsluta utan save
      if (digitalRead(buttonPin) == LOW) {
        delay(50); // mini-debounce
        if (digitalRead(buttonPin) == LOW) {
          Serial.println("Portal: avbruten via knapptryck");
          // Vänta på release så vi inte triggar ett nytt event direkt
          while (digitalRead(buttonPin) == LOW) delay(20);
          break;
        }
      }

      delay(50);
    }

    wm.stopConfigPortal();

    if (saved) {
      Serial.println("Portal: ny WiFi sparad");
      return true;
    }

    // Återanslut till befintlig WiFi efter portal-avslut
    Serial.println("Portal stängd, återansluter...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin();

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 100) {
      delay(100);
      retries++;
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Återanslutning misslyckades efter portal-avslut");
    } else {
      Serial.printf("Återansluten: IP=%s\n", WiFi.localIP().toString().c_str());
    }

    setTxPowerLow();
    return false;
  }

  void resetWiFiAndRestart() {
    Serial.println("Hard reset: raderar WiFi-credentials");
    WiFiManager wm;
    wm.resetSettings();
    delay(500);
    ESP.restart();
  }

  // --- NTP / tid ---
  void setupNTP() {
    // Svensk tidszon (CET/CEST med DST-regler)
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3",
                 "pool.ntp.org", "time.google.com");
    Serial.println("NTP konfigurerat (svensk tidszon)");
  }

  bool isTimeValid() {
    time_t now = time(nullptr);
    return now > 1700000000; // > 2023-11-14, dvs synkat
  }

  int getLocalHour() {
    if (!isTimeValid()) return -1;
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_hour;
  }

  String getLocalTimeString() {
    if (!isTimeValid()) return String("--:--");
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
    return String(buf);
  }

} // namespace TRESP
