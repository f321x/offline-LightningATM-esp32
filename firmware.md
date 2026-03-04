# Firmware Release Process (Copilot Workflow)

**âš ï¸ IMPORTANT: This is the authoritative guide for firmware releases. Follow the steps exactly.**

This document describes the workflow for building new firmware versions for all 5 display variants of the Offline-LightningATM-esp32.

---

## âš¡ Step 1 â€“ Get the current Bitcoin block height

```powershell
Invoke-WebRequest -Uri "https://mempool.space/api/blocks/tip/height" -UseBasicParsing | Select-Object -ExpandProperty Content
```

The returned value becomes the version number: `v<BLOCKHEIGHT>` (e.g. `v939148`)

---

## âš¡ Step 2 â€“ Update platformio.ini

In [platformio.ini](platformio.ini) set the comment and the build flag to the new version:

```ini
; Current version: v939148

build_flags =
    -DVERSION='"v939148"'
```

---

## âš¡ Step 3 â€“ For each of the 5 variants: adjust header â†’ build â†’ copy binaries â†’ manifest.json

The core configuration is in [include/GxEPD2_display_selection_new_style.h](include/GxEPD2_display_selection_new_style.h).  
There are **three locations** that must be active or commented out depending on the variant:

1. **Top (line ~33):** `USE_HSPI_FOR_EPD` and `ENABLE_GxEPD2_GFX 0` â€“ only active for `wv_esp32` variants
2. **Middle (line ~48â€“67):** The display driver `#define` + `const String display_type`
3. **Bottom in the ESP32 block (line ~206â€“214):** The `display(...)` constructor â€“ either std pinout (26,25,33,27) or Waveshare pinout (15,27,26,25)

> **âš ï¸ MANDATORY before every header change: verify the current state!**
>
> Always check what is currently active before editing the header.  
> Wrong assumptions about the current state cause duplicate `#define` errors in the build.
>
> ```powershell
> # Shows all active (non-commented) driver defines:
> Select-String -Path "include\GxEPD2_display_selection_new_style.h" -Pattern "^#define GxEPD2_DRIVER_CLASS|^#define USE_HSPI|^#define ENABLE_GxEPD2_GFX|^GxEPD2_DISPLAY_CLASS<" | Select-Object LineNumber, Line
> ```
>
> Expected output: **exactly one** `GxEPD2_DRIVER_CLASS` active, matching constructor active.  
> If more than one is active â†’ clean up manually first, then proceed.

---

### Variant 1 â€“ `Waveshare-2.13-v3-std_esp32`

**Display:** Waveshare 250Ã—122, 2.13" e-paper V3 | **Board:** Standard ESP32 (AZ Delivery pinout)

```cpp
// #define USE_HSPI_FOR_EPD          â† commented out
// #define ENABLE_GxEPD2_GFX 0       â† commented out

#define GxEPD2_DRIVER_CLASS GxEPD2_213_B74
const String display_type = "GxEPD2_213_B74";

// Constructor (std):
GxEPD2_DISPLAY_CLASS<...> display(...(/*CS=5*/ 26, /*DC=*/25, /*RST=*/33, /*BUSY=*/27));
// Constructor (wv) commented out
```

---

### Variant 2 â€“ `Waveshare-2.13-D-std_esp32`

**Display:** Waveshare 250Ã—122, 2.13" e-paper (D) flex (yellow) | **Board:** Standard ESP32

```cpp
// #define USE_HSPI_FOR_EPD          â† commented out
// #define ENABLE_GxEPD2_GFX 0       â† commented out

#define GxEPD2_DRIVER_CLASS GxEPD2_213_flex
const String display_type = "GxEPD2_213_flex";

// Constructor (std) active, (wv) commented out
```

---

### Variant 3 â€“ `Waveshare-2.7-v1-wv_esp32`

**Display:** Waveshare 264Ã—176, 2.7" E-Ink V1 | **Board:** Waveshare ESP32 Driver Board

```cpp
#define USE_HSPI_FOR_EPD              â† active
#define ENABLE_GxEPD2_GFX 0          â† active

#define GxEPD2_DRIVER_CLASS GxEPD2_270
const String display_type = "GxEPD2_270";

// Constructor (wv) active:
GxEPD2_DISPLAY_CLASS<...> display(...(/*CS=*/ 15, /*DC=*/ 27, /*RST=*/ 26, /*BUSY=*/ 25));
// Constructor (std) commented out
```

---

### Variant 4 â€“ `Waveshare-2.7-v2-wv_esp32`

**Display:** Waveshare 264Ã—176, 2.7" E-Ink V2 | **Board:** Waveshare ESP32 Driver Board

```cpp
#define USE_HSPI_FOR_EPD              â† active
#define ENABLE_GxEPD2_GFX 0          â† active

#define GxEPD2_DRIVER_CLASS GxEPD2_270_GDEY027T91
const String display_type = "GxEPD2_270_GDEY027T91";

// Constructor (wv) active, (std) commented out
```

---

### Variant 5 â€“ `Waveshare-1.54-v1-std_esp32`

**Display:** Waveshare 200Ã—200, 1.54" e-paper | **Board:** Standard ESP32

```cpp
// #define USE_HSPI_FOR_EPD          â† commented out
// #define ENABLE_GxEPD2_GFX 0       â† commented out

#define GxEPD2_DRIVER_CLASS GxEPD2_150_BN
const String display_type = "GxEPD2_150_BN";

// Constructor (std) active, (wv) commented out
```

---

## âš¡ Step 4 â€“ Run the build (after every header change)

```powershell
cd "d:\VSCode\offline-LightningATM-esp32-1"
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run
```

Build outputs are located at:
```
.pio\build\esp32dev\bootloader.bin
.pio\build\esp32dev\partitions.bin
.pio\build\esp32dev\firmware.bin
```

---

## âš¡ Step 5 â€“ Create firmware directory and copy binaries

```powershell
$v = "v939148"
$name = "Waveshare-2.13-v3-std_esp32"   # adjust per variant
$dir = "installer\firmware\${v}-${name}"
New-Item -ItemType Directory -Path $dir -Force
Copy-Item ".pio\build\esp32dev\bootloader.bin" $dir
Copy-Item ".pio\build\esp32dev\partitions.bin" $dir
Copy-Item ".pio\build\esp32dev\firmware.bin"   $dir
```

---

## âš¡ Step 6 â€“ Create manifest.json

Create a `manifest.json` for each new variant:

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

> **Note:** The `offset` for `bootloader.bin` is `4096` (not `0`) for standard ESP32 builds.

---

## âš¡ Step 7 â€“ Update installer/index.html

In [installer/index.html](installer/index.html) insert the **5 new `<option>` entries** at the top of the `<select>` dropdown (before the older versions):

```html
<option value="./firmware/v939148-Waveshare-2.13-v3-std_esp32/manifest.json">v939148-Waveshare-2.13-v3-std_esp32 (Latest âœ¨)</option>
<option value="./firmware/v939148-Waveshare-2.13-D-std_esp32/manifest.json">v939148-Waveshare-2.13-D-std_esp32 (Latest âœ¨)</option>
<option value="./firmware/v939148-Waveshare-2.7-v1-wv_esp32/manifest.json">v939148-Waveshare-2.7-v1-wv_esp32 (Latest âœ¨)</option>
<option value="./firmware/v939148-Waveshare-2.7-v2-wv_esp32/manifest.json">v939148-Waveshare-2.7-v2-wv_esp32 (Latest âœ¨)</option>
<option value="./firmware/v939148-Waveshare-1.54-v1-std_esp32/manifest.json">v939148-Waveshare-1.54-v1-std_esp32 (Latest âœ¨)</option>
```

Also update the default manifest of the `<esp-web-install-button>` to the latest version:

```html
<esp-web-install-button id="flash-button" manifest="./firmware/v939148-Waveshare-2.13-v3-std_esp32/manifest.json">
```

---

## Release overview

| Version   | Variant                         | Driver                   | Board type      |
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

## Pinout reference

| Board type     | CS   | DC   | RST  | BUSY |
|----------------|------|------|------|------|
| std_esp32      | 26   | 25   | 33   | 27   |
| wv_esp32       | 15   | 27   | 26   | 25   |

**wv_esp32** additionally requires:
```cpp
#define USE_HSPI_FOR_EPD
#define ENABLE_GxEPD2_GFX 0
```

