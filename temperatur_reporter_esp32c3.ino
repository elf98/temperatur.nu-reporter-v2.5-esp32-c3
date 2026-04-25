// temperatur_reporter_esp32c3.ino
//
// Temperatur.nu Reporter - ESP32-C3 0.42" OLED edition
// Migrated from Wemos D1 Mini (ESP8266) + 64x48 OLED
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
#include <DallasTemperature.h>
#include <esp_wifi.h>

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
const unsigned long MAINSCREEN_UPDATE_INTERVAL = 20;    // seconds between display refresh
const unsigned long DATA_SEND_INTERVAL         = 3 * 60; // seconds between uploads
const unsigned long DATA_SEND_SHORT_INTERVAL   = 1 * 60; // retry interval on failure

// --- WiFi AP name ---
const char* AP_NAME = "tnu_report";

// --- State ---
bool lastUploadSucceeded = true;
unsigned long lastFrameTime  = 0;
unsigned long lastUploadTime = 0;

// --- Button debounce ---
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 250;

void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off (active low)

  // Init display
  trDisplay.begin();
  trDisplay.bootScreen();

  // Print ESP info
  TRESP::printESPInfo();

  // Connect to WiFi
  trDisplay.connectingScreen("WiFi...");
  TRESP::setupWiFi(AP_NAME, APCallback, saveConfigCallback);

  // Återställ 1-Wire-pinnen från ev. UART-state innan OneWire tar över.
  // Utan detta hittar OneWire ibland inga sensorer på GPIO20 (RX0).
  pinMode(ONE_WIRE_BUS, OUTPUT);
  digitalWrite(ONE_WIRE_BUS, HIGH);
  pinMode(ONE_WIRE_BUS, INPUT);

  // Init sensors
  sensors.begin();
  sensors.setResolution(TEMPERATURE_PRECISION);

  // Initial display + upload
  mainScreen();
  sendDataToWeb();
}

void loop() {
  // Update display periodically
  if (timerDone(&lastFrameTime, MAINSCREEN_UPDATE_INTERVAL))
    mainScreen();

  // Send data periodically
  unsigned long interval = lastUploadSucceeded ? DATA_SEND_INTERVAL : DATA_SEND_SHORT_INTERVAL;
  if (timerDone(&lastUploadTime, interval))
    sendDataToWeb();

  // Button press cycles screen mode
  if (buttonPressed()) {
    trDisplay.nextScreenMode();
    mainScreen();
  }

  delay(100);
}

// --- WiFiManager AP callback ---
void APCallback(WiFiManager *myWiFiManager) {
  // Sänk sändningseffekt i AP-mode — telefonen är nära
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  trDisplay.APScreen(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback() {
  trDisplay.connectingScreen("Sparar...");
}

// --- Send temperature data to temperatur.nu ---
void sendDataToWeb() {
  sensors.begin();
  Serial.print("Sensors: ");
  Serial.println(sensors.getDeviceCount());

  lastUploadSucceeded = TRESP::sendDataToWeb(getLowestTemperature(), sensors.getDeviceCount());

  // Blink LED on upload
  digitalWrite(LED_PIN, LOW);
  delay(100);
  digitalWrite(LED_PIN, HIGH);

  if (lastUploadSucceeded) {
    Serial.printf("Upload OK, next in %lu s\n", DATA_SEND_INTERVAL);
  } else {
    Serial.printf("Upload FAILED, retry in %lu s\n", DATA_SEND_SHORT_INTERVAL);
  }
}

// --- Get lowest temperature from sensors ---
float getLowestTemperature() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (sensors.getDeviceCount() == 2)
    tempC = min(tempC, sensors.getTempCByIndex(1));
  return tempC;
}

// --- Update the display ---
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
    lastUploadSucceeded
  );
}

// --- Debounced button press detection ---
bool buttonPressed() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastButtonPress > DEBOUNCE_MS) {
      lastButtonPress = millis();
      // Wait for release
      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
      return true;
    }
  }
  return false;
}

// --- Timer helper ---
bool timerDone(unsigned long *lastUpdatedTime, unsigned long intervalTimeS) {
  if (millis() < *lastUpdatedTime + (intervalTimeS * 1000))
    return false;
  *lastUpdatedTime = millis();
  return true;
}
