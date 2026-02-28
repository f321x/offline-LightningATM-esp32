# Firmware Release Process

**⚠️ This is the authoritative source for firmware releases. Follow these steps exactly.**

## ⚡ Quick Start – Get current block height first

```powershell
Invoke-RestMethod -Uri "https://mempool.space/api/blocks/tip/height"
```

This becomes your version number: `vBLOCKHEIGHT` (e.g., `v938757`)

## Version Format

```
v<BLOCK_HEIGHT>-<Display-Model>-Pilot
```

Examples:
- `v938757-Waveshare-2.13-v3`  → Waveshare 2.13" e-paper version 3
- `v938757-Waveshare-2.7-v1-Pilot`   → Waveshare 2.7" E-Ink V.1

**Note:** The display type is compiled into the firmware, so each display type requires its own build and firmware binary.

## Release Steps

### 1. Update `platformio.ini`

```ini
build_flags =
    -DVERSION='"v938757-Waveshare-2.13-v3"'
```

Also ensure the correct display driver is selected in `include/GxEPD2_display_selection_new_style.h`
(or `include/lightning_atm.h` driver settings section).

### 2. Build with PlatformIO

```powershell
C:\Users\Datenrettung\.platformio\penv\Scripts\platformio.exe run
```

### 3. Create firmware directory

```powershell
mkdir installer/firmware/v938757-Waveshare-2.13-v3
```

### 4. Copy binaries

```powershell
Copy-Item .pio/build/esp32dev/bootloader.bin installer/firmware/v938757-Waveshare-2.13-v3/
Copy-Item .pio/build/esp32dev/partitions.bin  installer/firmware/v938757-Waveshare-2.13-v3/
Copy-Item .pio/build/esp32dev/firmware.bin    installer/firmware/v938757-Waveshare-2.13-v3/
```

### 5. Create `manifest.json`

```json
{
  "name": "Offline-LightningATM-esp32",
  "version": "v938757-Waveshare-2.13-v3",
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

### 6. Update `installer/index.html`

Update the `<esp-web-install-button>` manifest path and add the version to the `<select>` dropdown:

```html
<esp-web-install-button id="flash-button" manifest="./firmware/v938757-Waveshare-2.13-v3/manifest.json">
```

And in the `<select>`:
```html
<option value="./firmware/v938757-Waveshare-2.13-v3/manifest.json">
    v938757-Waveshare-2.13-v3 (Latest)
</option>
```

## Display Driver Reference

| Display | GxEPD2 driver class |
|---------|---------------------|
| Waveshare 1.54" | `GxEPD2_150_BN` |
| Waveshare 2.7" V.1 | `GxEPD2_270` |
| Waveshare 2.7" V.2 | `GxEPD2_270_GDEY027T91` |
| Waveshare 2.13" v3 | `GxEPD2_213_B74` |
| Waveshare 2.13" flex (YE) | `GxEPD2_213_flex` |
