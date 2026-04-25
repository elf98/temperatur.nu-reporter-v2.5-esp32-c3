# Temperatur.nu Reporter — ESP32-C3 (v2.5)

WiFi-temperaturrapportör för [temperatur.nu](https://www.temperatur.nu/), byggd kring **ESP32-C3 SuperMini med inbyggd 0.42" OLED**.

Läser DS18B20-sensor(er) över 1-Wire och rapporterar lägsta uppmätta temperatur till `report.temperatur.nu` med jämna mellanrum. Visar temperatur, station-PIN och WiFi-status på den inbyggda OLED-skärmen. WiFi konfigureras via captive portal (WiFiManager).

## Hårdvara

- **ESP32-C3 SuperMini** med inbyggd 0.42" OLED (72×40, SSD1306, I²C, hårdkodad SDA=GPIO5 / SCL=GPIO6)
- **DS18B20+** temperatursensor(er), upp till 2 st parallellkopplade på samma 1-Wire-buss
- **4.7 kΩ pull-up** mellan dataledning och 3V3 (3.3 kΩ fungerar också)

Se [KOPPLING.md](KOPPLING.md) för fullständigt kopplingsschema.

## Pinout

| Funktion         | GPIO | Anmärkning                                  |
|------------------|------|---------------------------------------------|
| 1-Wire (DS18B20) | 20   | RX0 — fri tack vare USB CDC On Boot         |
| OLED I²C SDA     | 5    | hårdkodat på board                          |
| OLED I²C SCL     | 6    | hårdkodat på board                          |
| Boot-knapp       | 9    | active LOW, intern pull-up                  |
| Onboard LED      | 8    | active LOW                                  |

## Bibliotek

Installeras via Arduino IDE Library Manager:

- **U8g2** (olikraus)
- **OneWire** (Paul Stoffregen)
- **DallasTemperature** (Miles Burton)
- **WiFiManager** (tzapu, version 2.x — ESP32-kompatibel)

## Arduino IDE-inställningar

| Inställning              | Värde                  |
|--------------------------|------------------------|
| Board                    | ESP32C3 Dev Module     |
| **USB CDC On Boot**      | **Enabled** *(viktigt)* |
| Upload Speed             | 921600                 |
| Flash Mode               | QIO                    |
| Partition Scheme         | Default 4 MB           |

> **OBS:** `USB CDC On Boot` *måste* vara `Enabled`. Annars använder `Serial.begin()` UART0 och låser GPIO20 → 1-Wire-bussen slutar fungera.

Lägg till boards-URL under Preferences:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

## Skärmlägen

Cykla mellan lägena med boot-knappen (GPIO9):

1. **Temperatur** — stor visning av lägsta uppmätta värde
2. **Sensorinfo** — båda sensorernas individuella temperaturer + antal
3. **PIN** — stationens PIN (chip-ID) för registrering på temperatur.nu
4. **WiFi** — signalstyrka som staplar

## Första uppstart

1. Flasha firmware
2. Kortet startar ett WiFi-AP `tnu_report` med IP `8.8.8.8` (triggar captive portal)
3. Anslut med mobil eller dator, välj ditt WiFi
4. Tryck på knappen tills PIN-skärmen visas — notera siffran
5. Registrera stationen på [temperatur.nu/nystation](https://www.temperatur.nu/nystation) med PIN-koden

## Protokoll

Skickar HTTP GET till:
```
http://report.temperatur.nu/rapportera_v2.php?pin=<chipid>&mac=<mac>&t=<temp>&n=<sensorcount>&u=<successcount>
```

## Effekthantering

- WiFi-TX sätts till **låg effekt (8 dBm)** mellan rapporteringar
- Höjs till **hög effekt (19.5 dBm)** precis under upload, sänks direkt efter
- Minskar effektförbrukning utan att kompromissa med upload-tillförlitlighet

## Filer

| Fil                                | Innehåll                                     |
|------------------------------------|----------------------------------------------|
| `temperatur_reporter_esp32c3.ino`  | Huvudfil, setup/loop, sensorhantering        |
| `TRESP.{h,cpp}`                    | WiFi, HTTP-rapportering, MAC/chip-ID         |
| `TRDisplay.{h,cpp}`                | OLED-rendering, skärmlägen                   |
| `wm_strings_sv.h`                  | Svenska strängar för WiFiManager-portalen    |
| `KOPPLING.md`                      | Kopplingsschema                              |
