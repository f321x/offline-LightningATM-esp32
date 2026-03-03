# Firmware Release Process (Copilot Workflow)

**⚠️ WICHTIG: Dies ist die maßgebliche Anleitung für Firmware-Releases. Schritte exakt befolgen.**

Dieses Dokument beschreibt den automatisierten Workflow zur Erstellung neuer Firmware-Versionen für alle 5 Display-Varianten des Offline-LightningATM-esp32.

---

## ⚡ Schritt 1 – Aktuelle Bitcoin-Blockhöhe ermitteln

```powershell
Invoke-WebRequest -Uri "https://mempool.space/api/blocks/tip/height" -UseBasicParsing | Select-Object -ExpandProperty Content
```

Der zurückgegebene Wert wird zur Versionsnummer: `v<BLOCKHÖHE>` (z. B. `v939148`)

---

## ⚡ Schritt 2 – platformio.ini aktualisieren

In [platformio.ini](platformio.ini) den Kommentar und den BUILD-FLAG auf die neue Version setzen:

```ini
; Current version: v939148

build_flags =
    -DVERSION='"v939148"'
```

---

## ⚡ Schritt 3 – Für jede der 5 Varianten: Header anpassen → Build → Binaries kopieren → manifest.json

Das Herzstück der Konfiguration liegt in [include/GxEPD2_display_selection_new_style.h](include/GxEPD2_display_selection_new_style.h).  
Es gibt **drei Stellen**, die je nach Variante aktiv oder auskommentiert sein müssen:

1. **Oben (Zeile ~33):** `USE_HSPI_FOR_EPD` und `ENABLE_GxEPD2_GFX 0` – nur für `wv_esp32`-Varianten aktiv
2. **Mitte (Zeile ~48–67):** Der Display-Treiber-`#define` + `const String display_type`
3. **Unten im ESP32-Block (Zeile ~206–214):** Der `display(...)`-Konstruktor – entweder std-Pinout (26,25,33,27) oder Waveshare-Pinout (15,27,26,25)

---

### Variante 1 – `Waveshare-2.13-v3-std_esp32`

**Display:** Waveshare 250×122, 2.13" e-paper V3 | **Board:** Standard ESP32 (AZ Delivery Pinout)

```cpp
// #define USE_HSPI_FOR_EPD          ← auskommentiert
// #define ENABLE_GxEPD2_GFX 0       ← auskommentiert

#define GxEPD2_DRIVER_CLASS GxEPD2_213_B74
const String display_type = "GxEPD2_213_B74";

// Konstruktor (std):
GxEPD2_DISPLAY_CLASS<...> display(...(/*CS=5*/ 26, /*DC=*/25, /*RST=*/33, /*BUSY=*/27));
// Konstruktor (wv) auskommentiert
```

---

### Variante 2 – `Waveshare-2.13-D-std_esp32`

**Display:** Waveshare 250×122, 2.13" e-paper (D) flex (gelb) | **Board:** Standard ESP32

```cpp
// #define USE_HSPI_FOR_EPD          ← auskommentiert
// #define ENABLE_GxEPD2_GFX 0       ← auskommentiert

#define GxEPD2_DRIVER_CLASS GxEPD2_213_flex
const String display_type = "GxEPD2_213_flex";

// Konstruktor (std) aktiv, (wv) auskommentiert
```

---

### Variante 3 – `Waveshare-2.7-v1-wv_esp32`

**Display:** Waveshare 264×176, 2.7" E-Ink V1 | **Board:** Waveshare ESP32 Driver Board

```cpp
#define USE_HSPI_FOR_EPD              ← aktiv
#define ENABLE_GxEPD2_GFX 0          ← aktiv

#define GxEPD2_DRIVER_CLASS GxEPD2_270
const String display_type = "GxEPD2_270";

// Konstruktor (wv) aktiv:
GxEPD2_DISPLAY_CLASS<...> display(...(/*CS=*/ 15, /*DC=*/ 27, /*RST=*/ 26, /*BUSY=*/ 25));
// Konstruktor (std) auskommentiert
```

---

### Variante 4 – `Waveshare-2.7-v2-wv_esp32`

**Display:** Waveshare 264×176, 2.7" E-Ink V2 | **Board:** Waveshare ESP32 Driver Board

```cpp
#define USE_HSPI_FOR_EPD              ← aktiv
#define ENABLE_GxEPD2_GFX 0          ← aktiv

#define GxEPD2_DRIVER_CLASS GxEPD2_270_GDEY027T91
const String display_type = "GxEPD2_270_GDEY027T91";

// Konstruktor (wv) aktiv, (std) auskommentiert
```

---

### Variante 5 – `Waveshare-1.54-v1-std_esp32`

**Display:** Waveshare 200×200, 1.54" e-paper | **Board:** Standard ESP32

```cpp
// #define USE_HSPI_FOR_EPD          ← auskommentiert
// #define ENABLE_GxEPD2_GFX 0       ← auskommentiert

#define GxEPD2_DRIVER_CLASS GxEPD2_150_BN
const String display_type = "GxEPD2_150_BN";

// Konstruktor (std) aktiv, (wv) auskommentiert
```

---

## ⚡ Schritt 4 – Build ausführen (nach jeder Header-Änderung)

```powershell
cd "d:\VSCode\offline-LightningATM-esp32-1"
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run
```

Build-Ausgaben liegen danach in:
```
.pio\build\esp32dev\bootloader.bin
.pio\build\esp32dev\partitions.bin
.pio\build\esp32dev\firmware.bin
```

---

## ⚡ Schritt 5 – Firmware-Verzeichnis anlegen und Binaries kopieren

```powershell
$v = "v939148"
$name = "Waveshare-2.13-v3-std_esp32"   # je Variante anpassen
$dir = "installer\firmware\${v}-${name}"
New-Item -ItemType Directory -Path $dir -Force
Copy-Item ".pio\build\esp32dev\bootloader.bin" $dir
Copy-Item ".pio\build\esp32dev\partitions.bin" $dir
Copy-Item ".pio\build\esp32dev\firmware.bin"   $dir
```

---

## ⚡ Schritt 6 – manifest.json erstellen

Für jede neue Variante eine `manifest.json` anlegen:

```json
{
  "name": "Offline-ATM",
  "version": "v939148-Waveshare-2.13-v3-std_esp32",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32",
      "parts": [
        { "path": "bootloader.bin", "offset": 4096 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "firmware.bin",   "offset": 65536 }
      ]
    }
  ]
}
```

> **Hinweis:** Der `offset` für `bootloader.bin` ist `4096` (nicht `0`) für Standard-ESP32-Builds.

---

## ⚡ Schritt 7 – installer/index.html aktualisieren

In [installer/index.html](installer/index.html) die **5 neuen `<option>`-Einträge** oben im `<select>`-Dropdown einfügen (vor den alten Versionen):

```html
<option value="./firmware/v939148-Waveshare-2.13-v3-std_esp32/manifest.json">v939148-Waveshare-2.13-v3-std_esp32 (Latest ✨)</option>
<option value="./firmware/v939148-Waveshare-2.13-D-std_esp32/manifest.json">v939148-Waveshare-2.13-D-std_esp32 (Latest ✨)</option>
<option value="./firmware/v939148-Waveshare-2.7-v1-wv_esp32/manifest.json">v939148-Waveshare-2.7-v1-wv_esp32 (Latest ✨)</option>
<option value="./firmware/v939148-Waveshare-2.7-v2-wv_esp32/manifest.json">v939148-Waveshare-2.7-v2-wv_esp32 (Latest ✨)</option>
<option value="./firmware/v939148-Waveshare-1.54-v1-std_esp32/manifest.json">v939148-Waveshare-1.54-v1-std_esp32 (Latest ✨)</option>
```

Außerdem den Default-Manifest des `<esp-web-install-button>` auf die neueste Version setzen:

```html
<esp-web-install-button id="flash-button" manifest="./firmware/v939148-Waveshare-2.13-v3-std_esp32/manifest.json">
```

---

## Release-Übersicht

| Version   | Variante                        | Treiber                  | Board-Typ       |
|-----------|--------------------------------|--------------------------|-----------------|
| v939148   | Waveshare-2.13-v3-std_esp32    | GxEPD2_213_B74           | std_esp32       |
| v939148   | Waveshare-2.13-D-std_esp32     | GxEPD2_213_flex          | std_esp32       |
| v939148   | Waveshare-2.7-v1-wv_esp32      | GxEPD2_270               | wv_esp32        |
| v939148   | Waveshare-2.7-v2-wv_esp32      | GxEPD2_270_GDEY027T91    | wv_esp32        |
| v939148   | Waveshare-1.54-v1-std_esp32    | GxEPD2_150_BN            | std_esp32       |
| v938844   | Waveshare-2.13-v3-std_esp32    | GxEPD2_213_B74           | std_esp32       |
| v938844   | Waveshare-2.13-D-std_esp32     | GxEPD2_213_flex          | std_esp32       |
| v938844   | Waveshare-2.7-v1-wv_esp32      | GxEPD2_270               | wv_esp32        |
| v938844   | Waveshare-2.7-v2-wv_esp32      | GxEPD2_270_GDEY027T91    | wv_esp32        |
| v938844   | Waveshare-1.54-v1-std_esp32    | GxEPD2_150_BN            | std_esp32       |

---

## Pinout-Referenz

| Board-Typ      | CS   | DC   | RST  | BUSY |
|----------------|------|------|------|------|
| std_esp32      | 26   | 25   | 33   | 27   |
| wv_esp32       | 15   | 27   | 26   | 25   |

**wv_esp32** erfordert zusätzlich:
```cpp
#define USE_HSPI_FOR_EPD
#define ENABLE_GxEPD2_GFX 0
```
