// wm_strings_sv.h
// Svenska strängar för WiFiManager v2.x
// Inkludera FÖRE WiFiManager.h via #define WIFI_MANAGER_OVERRIDE_STRINGS

#ifndef _WM_STRINGS_SV_H
#define _WM_STRINGS_SV_H

// Override WiFiManager default strings with Swedish translations
#define HTTP_HEAD_CL    "Content-Length"
#define HTTP_HEAD_CT    "text/html"
#define HTTP_HEAD_CORS  "Access-Control-Allow-Origin"

const char WM_HTTP_HEAD_START[]    PROGMEM = "<!DOCTYPE html><html lang='sv'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1,user-scalable=no'/><title>Temperatur.nu Reporter</title>";
const char WM_HTTP_STYLE[]         PROGMEM = "<style>"
  "body{font-family:Verdana,sans-serif;margin:0;padding:20px;background:#0a0a2e;color:#e0e0e0;}"
  "h1,h3{text-align:center;color:#4fc3f7;}"
  ".wrap{max-width:320px;margin:auto;}"
  "input[type=text],input[type=password]{width:100%;box-sizing:border-box;padding:10px;margin:5px 0;border:1px solid #444;border-radius:6px;background:#1a1a3e;color:#e0e0e0;font-size:16px;}"
  "button,.btn{width:100%;padding:12px;margin:8px 0;border:none;border-radius:6px;cursor:pointer;font-size:16px;font-weight:bold;}"
  "button,.btn{background:#1976d2;color:white;}"
  "button:hover,.btn:hover{background:#1565c0;}"
  ".btn.D{background:#c62828;}"
  ".btn.D:hover{background:#b71c1c;}"
  ".msg{padding:10px;border-radius:6px;margin:10px 0;}"
  ".msg.S{background:#1b5e20;border:1px solid #388e3c;}"
  ".msg.D{background:#b71c1c;border:1px solid #c62828;}"
  "a{color:#4fc3f7;}"
  ".q{float:right;}"
  "</style>";
const char WM_HTTP_SCRIPT[]        PROGMEM = "<script>function c(l){document.getElementById('s').value=l.getAttribute('data-ssid')||l.innerText||l.textContent;p=l.nextElementSibling.classList.contains('l');document.getElementById('p').type=p?'password':'text';}</script>";
const char WM_HTTP_HEAD_END[]      PROGMEM = "</head><body><div class='wrap'>";
const char WM_HTTP_ROOT_MAIN[]     PROGMEM = "<h1>Temperatur.nu</h1><h3>Reporter WiFi</h3>";
const char WM_HTTP_PORTAL_OPTIONS[] PROGMEM = "<form action='/wifi' method='get'><button>Konfigurera WiFi</button></form><br/><form action='/info' method='get'><button>Information</button></form><br/><form action='/restart' method='get'><button class='btn D'>Starta om</button></form>";
const char WM_HTTP_ITEM_QI[]       PROGMEM = "<div role='img' aria-label='{r}%' title='{r}%' class='q q-{q} {i} {h}'></div>";
const char WM_HTTP_ITEM_QP[]       PROGMEM = "<div class='q q-{q} {i} {h}'></div>";
const char WM_HTTP_ITEM[]          PROGMEM = "<div><a href='#p' onclick='c(this)' data-ssid='{v}'>{v}</a> {qi}<div>";
const char WM_HTTP_FORM_START[]    PROGMEM = "<form method='POST' action='/wifisave'><label for='s'>SSID</label><input id='s' name='s' maxlength='32' autocorrect='off' autocapitalize='none' placeholder='Natverksnamn' type='text'><br/><label for='p'>Losenord</label><input id='p' name='p' maxlength='64' type='password' placeholder='Losenord'><br/>";
const char WM_HTTP_FORM_END[]      PROGMEM = "<br/><button type='submit'>Spara</button></form>";
const char WM_HTTP_SAVED[]         PROGMEM = "<div class='msg S'>Sparar uppgifter och ansluter.<br/>Om det misslyckas, anslut till AP:n igen.</div>";
const char WM_HTTP_END[]           PROGMEM = "</div></body></html>";
const char WM_HTTP_ERASEBTN[]      PROGMEM = "<br/><form action='/erase' method='get'><button class='btn D'>Radera WiFi-inst&auml;llningar</button></form>";
const char WM_HTTP_UPDATEBTN[]     PROGMEM = "<br/><form action='/update' method='get'><button>Uppdatera firmware</button></form>";
const char WM_HTTP_BACKBTN[]       PROGMEM = "<hr><br/><form action='/' method='get'><button>Tillbaka</button></form>";
const char WM_HTTP_STATUS_ON[]     PROGMEM = "<div class='msg S'><strong>Ansluten</strong> till {v}<br/><em>{0}</em></div>";
const char WM_HTTP_STATUS_OFF[]    PROGMEM = "<div class='msg D'><strong>Ej ansluten</strong></div>";

#endif // _WM_STRINGS_SV_H
