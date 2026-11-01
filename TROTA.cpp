// TROTA.cpp
#include "TROTA.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

namespace TROTA {

// Egen kanal per partitionsschema: enheter med gamla Default 4 MB-tabellen
// (1,25 MB app) läser esp32c3/ som är fryst på 2.5.0. OTA kan inte byta
// partitionstabell, så de måste USB-flashas för att komma hit.
const char* VERSION_URL       = "http://report.temperatur.nu/firmware/esp32c3-nofs/version.txt";
const char* FIRMWARE_URL_BASE = "http://report.temperatur.nu/firmware/esp32c3-nofs/";

String fetchLatestVersion() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA: ej ansluten");
    return String();
  }

  WiFiClient client;
  HTTPClient http;

  if (!http.begin(client, VERSION_URL)) {
    Serial.println("OTA: http.begin failed");
    return String();
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("OTA: version fetch HTTP %d\n", code);
    http.end();
    return String();
  }

  String latest = http.getString();
  latest.trim();
  http.end();
  return latest;
}

Result downloadAndFlash(const String& version) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("OTA: ej ansluten vid flash");
    return Result::UPDATE_FAILED;
  }

  String firmwareUrl = String(FIRMWARE_URL_BASE) + "firmware-" + version + ".bin";
  Serial.printf("OTA: hämtar %s...\n", firmwareUrl.c_str());

  WiFiClient client;
  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("OTA failed (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      return Result::UPDATE_FAILED;
    case HTTP_UPDATE_NO_UPDATES:
      return Result::NO_UPDATE;
    case HTTP_UPDATE_OK:
      Serial.println("OTA OK — rebootar");
      return Result::UPDATED;
  }
  return Result::UPDATE_FAILED;
}

Result checkAndUpdate(const char* currentVersion) {
  Serial.println("--- OTA check ---");
  String latest = fetchLatestVersion();
  if (latest.length() == 0) return Result::CHECK_FAILED;

  Serial.printf("OTA: current=%s, latest=%s\n", currentVersion, latest.c_str());

  if (latest == String(currentVersion)) {
    Serial.println("OTA: aktuell version redan installerad");
    return Result::NO_UPDATE;
  }

  return downloadAndFlash(latest);
}

} // namespace TROTA
