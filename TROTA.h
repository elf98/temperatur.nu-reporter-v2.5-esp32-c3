// TROTA.h
// Pull-OTA för Temperatur.nu Reporter ESP32-C3
#ifndef TROTA_H
#define TROTA_H

#include <Arduino.h>

namespace TROTA {
  extern const char* VERSION_URL;
  extern const char* FIRMWARE_URL_BASE; // utan filnamn, slutar med /

  enum class Result {
    UPDATED,        // (returnerar inte — rebootar)
    NO_UPDATE,      // version aktuell
    CHECK_FAILED,   // kunde inte nå/läsa version.txt
    UPDATE_FAILED   // version ny men nedladdning/flash misslyckades
  };

  // Hämtar latest version från servern. Returnerar tom sträng vid fel.
  String fetchLatestVersion();

  // Hämtar firmware-<version>.bin och flashar. Rebootar vid success
  // (returnerar inte i success-fallet).
  Result downloadAndFlash(const String& version);

  // Convenience wrapper: kollar version, hämtar om olikt. För tysta
  // periodiska checks utan display-feedback.
  Result checkAndUpdate(const char* currentVersion);
}

#endif
