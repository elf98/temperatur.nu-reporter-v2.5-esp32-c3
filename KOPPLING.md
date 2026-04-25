# Temperatur.nu Reporter - ESP32-C3 0.42" OLED

## Kopplingsschema

```
ESP32-C3 0.42" OLED        DS18B20+
─────────────────────       ─────────
GPIO20 ──────────────────── DATA (gul/blå)     [RX0-pinnen]
3V3    ──────────────────── VCC  (röd/orange)
GND    ──────────────────── GND  (svart/blåvit+orangevit)
       │
       └── 4.7kΩ mellan GPIO20 och 3V3 (pull-up)

Inbyggd OLED (72x40):  SDA=GPIO5, SCL=GPIO6 (hardwired)
Inbyggd knapp:         GPIO9 (boot-knapp)
Inbyggd LED:           GPIO8
```

## Skärmlägen (cykla med knapp)

1. **Temperatur** - Stor temperaturvisning (default)
2. **Sensorinfo** - Båda sensorernas temperaturer + antal
3. **PIN** - Stations-PIN (chip-ID för registrering på temperatur.nu)
4. **WiFi-status** - Signalstyrka som staplar

## Arduino IDE-inställningar

- **Board:** ESP32C3 Dev Module
- **USB CDC On Boot:** Enabled
- **Upload Speed:** 921600
- **Flash Mode:** QIO
- **Partition Scheme:** Default 4MB

## Bibliotek att installera (via Library Manager)

- **U8g2** (av olikraus)
- **OneWire** (av Paul Stoffregen)
- **DallasTemperature** (av Miles Burton)
- **WiFiManager** (av tzapu, version 2.x - ESP32-kompatibel)

## Board-URL för Arduino IDE

Lägg till i Preferences > Additional boards manager URLs:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

## Första uppstart

1. Flashar firmware
2. Kortet skapar WiFi-nätverk "Temperatur_reporter"
3. Anslut till det med mobilen/datorn
4. Konfigurera ditt WiFi
5. Notera PIN-koden (tryck knappen tills PIN-skärmen visas)
6. Registrera stationen på www.temperatur.nu/nystation med PIN

## OBS: Ny PIN

ESP32-C3 genererar ett annat chip-ID än ESP8266. Du behöver registrera
stationen på nytt hos temperatur.nu med det nya PIN-numret.
