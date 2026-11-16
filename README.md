# Temperatur.nu Reporter v2.5 (ESP32-C3)

Wifi-temperaturrapportör för [temperatur.nu](https://www.temperatur.nu/), byggd kring ett **ESP32-C3-kort med inbyggd 0,42" OLED-display**.

Reportern läser en eller två DS18B20-sensorer över 1-Wire och rapporterar den lägsta temperaturen till temperatur.nu varannan minut. Displayen visar temperatur, PIN-kod och wifi-status. Wifi ställs in på en inställningssida på svenska, och ny mjukvara hämtar reportern själv.

- **Köpa färdig eller köpa delarna:** [butiken på temperatur.nu](https://www.temperatur.nu/butik/)
- **Bygga själv, steg för steg med bilder:** [byggbeskrivningen](https://www.temperatur.nu/info/bygg-din-egen-temperatur-nu-reporter-v2-5/)
- **Använda reportern:** [manualen](https://www.temperatur.nu/info/manual-till-temperatur-nu-reporter-v2-5/)

## Installera mjukvaran

Det finns två sätt.

### 1. Från webbläsaren (enklast)

Gå till [temperatur.nu/reporter/flasha](https://www.temperatur.nu/reporter/flasha/) i Chrome eller Edge på en dator, anslut kortet med USB-C och klicka på knappen. Det tar ungefär en minut och kräver inga program. Installationen raderar ett sparat wifi-nät, så reportern startar med inställningssidan.

### 2. Bygg själv i Arduino IDE

För dig som vill läsa eller ändra koden.

1. Installera [Arduino IDE](https://www.arduino.cc/en/software) 2.x.
2. Lägg till den här adressen under *Arkiv/Inställningar*, "Additional boards manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Installera **esp32 by Espressif Systems** i korthanteraren (testat med 3.3.12).
4. Installera biblioteken **U8g2** (olikraus), **OneWire** (Paul Stoffregen) och **DallasTemperature** (Miles Burton).
   WiFiManager ska *inte* installeras, se [Svenska texter i inställningssidan](#svenska-texter-i-inställningssidan).
5. Ladda ner koden och se till att mappen heter `temperatur_reporter_esp32c3`, samma som ino-filen.
6. Välj kortet **ESP32C3 Dev Module** och ställ in:

   | Inställning          | Värde                          |
   |----------------------|--------------------------------|
   | **USB CDC On Boot**  | **Enabled**                    |
   | **Partition Scheme** | **No FS 4MB (2MB APP x2)**     |
   | Upload Speed         | 921600                         |
   | Flash Mode           | QIO                            |

7. Ladda upp.

De två fetmarkerade inställningarna är nödvändiga:

- **USB CDC On Boot: Enabled.** Annars tar serieporten GPIO20, som 1-Wire-bussen använder, och sensorerna hittas inte.
- **Partition Scheme: No FS 4MB.** Annars ryms inte uppdateringarna, och reportern kan inte uppdatera sig själv.

## Hårdvara

- **ESP32-C3-kort** med inbyggd 0,42" OLED (72 × 40 pixlar, SSD1306, I²C)
- **DS18B20+**, en eller två, parallellt på samma 1-Wire-buss
- **Pull-up-motstånd** på 3,3–4,7 kΩ mellan dataledningen och 3,3 V

| Funktion         | GPIO | Anmärkning                                   |
|------------------|------|----------------------------------------------|
| 1-Wire (DS18B20) | 20   | stiftet märkt RX; fritt tack vare USB CDC    |
| OLED I²C SDA     | 5    | fast kopplad på kortet                       |
| OLED I²C SCL     | 6    | fast kopplad på kortet                       |
| Knapp (BOOT)     | 9    | aktiv låg, intern pull-up                    |
| Lysdiod          | 8    | aktiv låg                                    |

### Koppling

```
ESP32-C3-kortet               DS18B20+
───────────────               ────────
RX  (GPIO20) ───────────────── data
V3  (3,3 V)  ───────────────── VDD
GD  (GND)    ───────────────── GND
        │
        └── motstånd 3,3–4,7 kΩ mellan V3 och RX (pull-up)
```

Stiften heter V3, GD och RX på kortet. Två sensorer kopplas parallellt på samma tre ledare, och ett motstånd räcker för båda. Med partvinnad kabel passar blå till RX, orange till V3 och de två vitrandiga ledarna till GD. Kabeln kan vara upp till ungefär 20 meter sammanlagt.

Motståndet är ytmonterat (1206) och löds mellan hålen V3 och RX på kortets skärmsida. Bilder på varje steg finns i [byggbeskrivningen](https://www.temperatur.nu/info/bygg-din-egen-temperatur-nu-reporter-v2-5/). En låda att skriva ut finns i [hardware/](hardware/).

## Första uppstart

1. Reportern startar wifi-nätet **tnu_report** och visar det på displayen.
2. Anslut en telefon eller dator till nätet. Inställningssidan öppnas oftast av sig själv, annars på adressen `8.8.8.8`.
3. Överst på sidan står reporterns **PIN-kod** och hur många sensorer den har hittat, med deras temperaturer.
4. Tryck på *Skanna efter nätverk*, välj ditt wifi (2,4 GHz), skriv lösenordet och tryck på *Spara*.
5. Anmäl mätpunkten på [temperatur.nu/nymatpunkt](https://www.temperatur.nu/nymatpunkt/) med PIN-koden.

Medan inställningssidan är öppen kan du trycka på knappen för att se sensorernas temperaturer på displayen. Inställningssidan stänger sig själv efter fem minuter om ett wifi redan är sparat.

## Knappen och displayen

Ett kort tryck växlar mellan fyra lägen. Efter en minut går displayen tillbaka till temperaturen.

1. **Temperatur**: värdet som rapporteras, alltså det lägsta av sensorernas
2. **Sensorinfo**: varje sensors temperatur för sig
3. **PIN**: reporterns PIN-kod
4. **WiFi**: klocka, mjukvaruversion och signalstyrka

Håller du in knappen i WiFi-läget visar displayen vad som händer när du släpper:

| Hålltid   | Händelse                                                       |
|-----------|----------------------------------------------------------------|
| 1–5 s     | **OTA**: söker efter ny mjukvara direkt                        |
| 5–10 s    | **Portal**: öppnar inställningssidan igen                      |
| 10–15 s   | **AVBRYT**: ingenting händer                                   |
| 15–20 s   | **RESET**: raderar det sparade wifi-nätet                      |
| över 20 s | **AVBRYT**: ingenting händer                                   |

## Uppdateringar (OTA)

Reportern söker efter ny mjukvara vid start och en gång per dygn mellan kl. 02 och 05. Finns en ny version hämtar den filen, installerar den och startar om. Versionen står i `FW_VERSION` i ino-filen och syns i WiFi-läget på displayen.

## Rapportering

Reportern skickar ett HTTP GET-anrop varannan minut (efter ett misslyckat försök redan efter en minut):

```
http://report.temperatur.nu/rapportera_v2.php?pin=<pin>&mac=<mac>&fw=<version>&t=<lägsta>&n=<antal sensorer>&u=<räknare>&t1=<sensor 1>&t2=<sensor 2>
```

PIN-koden bildas av de tre sista byten i kortets MAC-adress och är densamma varje gång.

För att hålla nere egenvärmen sänder wifi-radion med låg effekt mellan rapporterna och höjer den bara under själva uppladdningen. Processorn går på 80 MHz, det lägsta som wifi tillåter.

## Svenska texter i inställningssidan

Inställningssidan kommer från biblioteket [WiFiManager](https://github.com/tzapu/WiFiManager) (tzapu, MIT-licens). Det väljer språkfil när det kompileras, och ett bibliotek som installerats i Arduino IDE går inte att ge en egen språkfil. Därför ligger version 2.0.17 med i koden, i `src/WiFiManager/`, med den svenska filen `wm_strings_sv.h`.

Vid uppgradering av biblioteket: ersätt filerna i `src/WiFiManager/`, behåll `wm_strings_sv.h` och raden `WM_STRINGS_FILE` i `WiFiManager.h`, och för över nya texter från `wm_strings_en.h`.

## Filer

| Fil                                | Innehåll                                                        |
|------------------------------------|-----------------------------------------------------------------|
| `temperatur_reporter_esp32c3.ino`  | Huvudfil: start, huvudloop, sensorer, knapp, OTA-schema         |
| `TRESP.{h,cpp}`                    | Wifi, inställningssidan, rapportering, PIN-kod                  |
| `TRDisplay.{h,cpp}`                | Displayen och visningslägena                                    |
| `TROTA.{h,cpp}`                    | Hämtar och installerar ny mjukvara                              |
| `src/WiFiManager/`                 | WiFiManager 2.0.17 med svenska texter                           |
| `build/…/*.ino.bin`                | Senaste släppta bygget                                          |
| `hardware/`                        | STL-fil för lådan                                               |
