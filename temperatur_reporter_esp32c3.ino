// temperatur_reporter_esp32c3.ino
//
// Temperatur.nu Reporter - ESP32-C3 0.42" OLED edition
//
// Hardware:
//   ESP32-C3 SuperMini with 0.42" OLED (72x40, SSD1306, I2C)
//   DS18B20+ temperature sensors (up to 2)
//
// Libraries needed:
//   U8g2           - OLED display
//   OneWire        - 1-Wire protocol
//   DallasTemperature - DS18B20 sensors
//   WiFiManager    - WiFi config portal (tzapu, ESP32 version)

#include "TRESP.h"
#include "TRDisplay.h"
#include "TROTA.h"
#include <DallasTemperature.h>
#include <esp_wifi.h>

// --- Firmware version ---
#define FW_VERSION "2.6.0"

// --- Dev/prod-toggle för OTA-intervall ---
//   1 = check varje minut (utveckling)
//   0 = check en gång per dygn nattetid (produktion)
#define DEV_MODE 0

// --- Pin definitions ---
#define ONE_WIRE_BUS   20  // GPIO20 (RX0)
#define BUTTON_PIN     9   // GPIO9 - boot button on the board (active LOW, internal pull-up)
#define LED_PIN        8   // GPIO8 - onboard blue LED (active LOW)

// --- Temperature sensor setup ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
#define TEMPERATURE_PRECISION 12

// --- Display ---
TRDisplay trDisplay;

// --- Timing ---
const unsigned long MAINSCREEN_UPDATE_INTERVAL = 20;     // sek mellan display-refresh
const unsigned long DATA_SEND_INTERVAL         = 120;    // sek mellan upload
const unsigned long DATA_SEND_SHORT_INTERVAL   = 1 * 60; // retry vid fail

// OTA-intervall (sek)
#if DEV_MODE
const unsigned long OTA_CHECK_INTERVAL = 60;            // 1 min
#else
const unsigned long OTA_CHECK_INTERVAL = 24 * 3600;     // 24 h
#endif

// --- WiFi AP name ---
const char* AP_NAME = "tnu_report";

// --- State ---
bool lastUploadSucceeded = true;
unsigned long lastFrameTime    = 0;
unsigned long lastUploadTime   = 0;
unsigned long lastOTACheck     = 0;
unsigned long lastInteractionMs = 0;
int lastOTACheckDay = -1;  // för dygns-baserad check i prod

// Auto-retur till temperaturskärmen efter inaktivitet
const unsigned long SCREEN_AUTO_RETURN_MS = 60UL * 1000UL;

// --- Button (ISR + polling vid press) ---
volatile unsigned long pressStartMs = 0;
volatile bool pressDetected = false;
const unsigned long DEBOUNCE_MS = 50;

void IRAM_ATTR buttonISR() {
  unsigned long now = millis();
  if (!pressDetected && (now - pressStartMs) > DEBOUNCE_MS) {
    pressStartMs = now;
    pressDetected = true;
  }
}

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== Temperatur.nu Reporter v%s ===\n", FW_VERSION);

  TRESP::setFwVersion(FW_VERSION);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  trDisplay.begin();
  trDisplay.bootScreen();

  TRESP::printESPInfo();

  trDisplay.connectingScreen("WiFi...");
  TRESP::setupWiFi(AP_NAME, APCallback, saveConfigCallback);

  TRESP::setupNTP();

  // Återställ 1-Wire-pinnen från ev. UART-state innan OneWire tar över.
  pinMode(ONE_WIRE_BUS, OUTPUT);
  digitalWrite(ONE_WIRE_BUS, HIGH);
  pinMode(ONE_WIRE_BUS, INPUT);

  sensors.begin();
  sensors.setResolution(TEMPERATURE_PRECISION);

  mainScreen();

  // OTA-check vid uppstart — plockar upp ny FW direkt efter boot/USB-flash
  // istället för att vänta in första timer-tick.
  doOTACheck(false);

  sendDataToWeb();
}

void loop() {
  if (timerDone(&lastFrameTime, MAINSCREEN_UPDATE_INTERVAL))
    mainScreen();

  unsigned long interval = lastUploadSucceeded ? DATA_SEND_INTERVAL : DATA_SEND_SHORT_INTERVAL;
  if (timerDone(&lastUploadTime, interval))
    sendDataToWeb();

  if (otaCheckDue())
    runOTACheck();

  if (pressDetected)
    handleButtonPress();

  // Auto-retur till temperaturskärmen efter inaktivitet
  if (trDisplay.getScreenMode() != SCREEN_TEMPERATURE &&
      millis() - lastInteractionMs > SCREEN_AUTO_RETURN_MS) {
    trDisplay.setScreenMode(SCREEN_TEMPERATURE);
    mainScreen();
  }

  delay(20);
}

// --- Knapp-hantering: kort = cykla, långtryck (på WiFi-skärm) = admin ---
void handleButtonPress() {
  unsigned long pressStart = pressStartMs;
  bool onWifiScreen = (trDisplay.getScreenMode() == SCREEN_WIFI);

  // Polla tills release
  while (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long held = millis() - pressStart;
    if (held > 1000 && onWifiScreen) {
      trDisplay.showLongPressIndicator(held);
    }
    delay(40);
  }

  unsigned long duration = millis() - pressStart;
  pressDetected = false;
  lastInteractionMs = millis();

  Serial.printf("Knapp: %lu ms (skärm=%d)\n", duration, trDisplay.getScreenMode());

  // Kort tryck eller på annan skärm = cykla
  if (duration < 1000 || !onWifiScreen) {
    trDisplay.nextScreenMode();
    mainScreen();
    return;
  }

  // Långtryck på WiFi-skärm
  // Stages: OTA -> Portal -> AVBRYT (säker exit) -> RESET -> AVBRYT
  // Sista AVBRYT: den som håller kvar för länge ska inte hamna i RESET ändå.
  if (duration < 5000) {
    runManualOTACheck();
    mainScreen();
  } else if (duration < 10000) {
    runManualPortal();
    mainScreen();
  } else if (duration < 15000 || duration >= 20000) {
    // AVBRYT — säker exit, ingen action
    trDisplay.statusScreen("Avbrutet", "");
    delay(1000);
    mainScreen();
  } else {
    trDisplay.statusScreen("RESET", "Raderar WiFi");
    delay(1500);
    TRESP::resetWiFiAndRestart();
    // kommer inte hit
  }
}

// Gemensamt OTA-flöde.
// verbose=true:  visar alla steg på display (Soker / Ingen ny FW / Server ej nadd / FW-fel)
// verbose=false: tyst på display när inget nytt — visar bara "Ny FW hittad" + "Hamtar..."
// Serial-logg är alltid aktiv oavsett verbose, så vi alltid kan följa flödet.
void doOTACheck(bool verbose) {
  Serial.printf("OTA check (verbose=%d): current=%s\n", verbose, FW_VERSION);

  if (verbose) {
    trDisplay.statusScreen("Soker FW", "...");
  }

  TRESP::setTxPowerHigh();
  delay(50);

  String latest = TROTA::fetchLatestVersion();

  if (latest.length() == 0) {
    Serial.println("OTA: server ej nadd / version.txt tom");
    if (verbose) {
      trDisplay.statusScreen("Server", "ej nadd");
      delay(2500);
    }
    TRESP::setTxPowerLow();
    return;
  }

  Serial.printf("OTA: latest=%s\n", latest.c_str());

  if (latest == FW_VERSION) {
    Serial.println("OTA: ingen ny version");
    if (verbose) {
      trDisplay.statusScreen("Ingen ny FW", FW_VERSION);
      delay(2500);
    }
    TRESP::setTxPowerLow();
    return;
  }

  // Ny version — visa alltid (även vid auto-check, så reboot inte är mystiskt)
  Serial.printf("OTA: ny version %s, hamtar...\n", latest.c_str());
  trDisplay.statusScreen("Ny FW hittad", latest.c_str());
  delay(1500);
  trDisplay.statusScreen("Hamtar...", latest.c_str());

  TROTA::Result r = TROTA::downloadAndFlash(latest);
  // UPDATED → rebootade redan, kommer inte hit
  TRESP::setTxPowerLow();

  if (r == TROTA::Result::UPDATE_FAILED) {
    trDisplay.statusScreen("FW-fel", "se serial");
    delay(3000);
  }
}

void runManualOTACheck() { doOTACheck(true); }

// Manuellt öppnad config-portal. Återgår till normal drift om avslutad utan save.
void runManualPortal() {
  trDisplay.statusScreen("Portal", "Knapp=ut");
  bool saved = TRESP::startConfigPortal(AP_NAME, APCallback, saveConfigCallback, BUTTON_PIN);
  if (saved) {
    trDisplay.statusScreen("WiFi sparad", "Reboot");
    delay(1000);
    ESP.restart();
  } else {
    trDisplay.statusScreen("Portal av", "");
    delay(1500);
  }
  // Knapptrycket som avslutade portalen kan ha satt ISR-flaggan — ignorera det
  pressDetected = false;
}

// --- OTA: avgör om det är dags att kolla ---
bool otaCheckDue() {
#if DEV_MODE
  return timerDone(&lastOTACheck, OTA_CHECK_INTERVAL);
#else
  // Prod: en gång per dygn mellan 02:00–04:00 lokal tid
  int hour = TRESP::getLocalHour();
  if (hour < 0) return false;  // tid ej giltig än

  if (hour < 2 || hour > 4) return false;

  // Dagens datum — bara en check per dygn
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  if (t.tm_yday == lastOTACheckDay) return false;

  lastOTACheckDay = t.tm_yday;
  return true;
#endif
}

// Periodisk (timer-driven) OTA-check.
// Tyst när inget nytt — visar "Ny FW hittad" + "Hamtar..." när update sker.
void runOTACheck() {
  Serial.println("--- Periodisk OTA-check ---");
  doOTACheck(false);
}

// --- WiFiManager AP callback ---
void APCallback(WiFiManager *myWiFiManager) {
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  trDisplay.APScreen(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback() {
  trDisplay.connectingScreen("Sparar...");
}

void sendDataToWeb() {
  sensors.begin();
  int sensorCount = sensors.getDeviceCount();
  Serial.print("Sensors: ");
  Serial.println(sensorCount);

  // Läs alla sensorer en gång; getLowestTemperature() gör en extra request annars
  sensors.requestTemperatures();
  float t1 = sensors.getTempCByIndex(0);
  float t2 = (sensorCount >= 2) ? sensors.getTempCByIndex(1) : -127.0f;
  float lowest = (sensorCount >= 2) ? min(t1, t2) : t1;

  lastUploadSucceeded = TRESP::sendDataToWeb(lowest, sensorCount, t1, t2);

  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);

  if (lastUploadSucceeded)
    Serial.printf("Upload OK, next in %lu s\n", DATA_SEND_INTERVAL);
  else
    Serial.printf("Upload FAILED, retry in %lu s\n", DATA_SEND_SHORT_INTERVAL);
}

float getLowestTemperature() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (sensors.getDeviceCount() == 2)
    tempC = min(tempC, sensors.getTempCByIndex(1));
  return tempC;
}

void mainScreen() {
  sensors.begin();
  int strength = map(TRESP::getRSSIasPercentage(), 0, 100, 0, 4);

  sensors.requestTemperatures();
  float temperatures[2] = { sensors.getTempCByIndex(0), sensors.getTempCByIndex(1) };

  trDisplay.mainScreen(
    strength,
    temperatures,
    sensors.getDeviceCount(),
    TRESP::getChipId(),
    WiFi.SSID(),
    WiFi.localIP().toString(),
    TRESP::getRSSIasPercentage(),
    lastUploadSucceeded,
    FW_VERSION,
    TRESP::getLocalTimeString()
  );
}

bool timerDone(unsigned long *lastUpdatedTime, unsigned long intervalTimeS) {
  if (millis() < *lastUpdatedTime + (intervalTimeS * 1000))
    return false;
  *lastUpdatedTime = millis();
  return true;
}
