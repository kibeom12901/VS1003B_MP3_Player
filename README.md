# 🎧 VS1003B MP3 Player — STM32F4 HAL-Based Driver Demo

<img width="703" height="533" alt="VS1003B MP3 Player Hardware Setup" src="https://github.com/user-attachments/assets/e021b8f2-c509-44d5-8b85-8545967e1c4c" />

This repository demonstrates a **complete MP3 player system** built on the **STM32F407 Discovery** board and the **VS1003B audio decoder**, integrating SPI, SD/FatFS, and user control logic step by step.  
Each stage under [`/stages`](./stages) represents a milestone in functionality — from basic SPI connection tests to a fully functional, playlist-based MP3 player with shuffle and gesture support.

---

## 📂 Directory Overview

| Folder | Description |
|:--|:--|
| [`hardware/`](./hardware) | Circuit diagrams, pin mappings, and VS1003B/SD card wiring references. |
| [`lib/`](./lib) | Reusable driver modules (LCD, UART logger, VS10xx helpers, etc.). |
| [`docs/`](./docs) | Notes, schematics, and reference documents. |
| [`stages/`](./stages) | Progressive firmware stages from 01–06, each introducing new subsystems. |

---

## 🧩 Stage Progression

| Stage | Title | Description |
|:--|:--|:--|
| **01** | [connection-test](./stages/01-connection-test) | SPI1 communication test with VS1003B (verify DREQ, XCS, XDCS). |
| **02** | [mp3-playback](./stages/02-mp3-playback) | Play a built-in MP3 sample from flash memory. |
| **03** | [sdcard-connection-test](./stages/03-sdcard-connection-test) | SPI2 SD card initialization (CMD0/ACMD41) verification. |
| **04** | [sdcard-fatfs](./stages/04-sdcard-fatfs) | Mount the SD card via FatFS and list files. |
| **05** | [vs1003-fatfs-playlist](./stages/05-vs1003-fatfs-playlist) | Integrate VS1003B decoding + FatFS file streaming. |
| **06** | [vs1003-fatfs-refinements](./stages/06-vs1003-fatfs-refinements) | Add shuffle mode, SD remount retry, and button gestures (single/double/long press). |
| **07** | [FINAL-modularized-lib](./stages/07-FINAL-modularized-lib) | Final modular version — all reusable drivers moved to `/lib` (VS1003, playlist, button, volume, util, etc.) and `main.c` simplified for clean structure. |

---

## ⚙️ Hardware Setup

| Component | Interface | Description |
|:--|:--|:--|
| **VS1003B (Audio Decoder)** | SPI1 | Handles MP3 decoding; controlled via SCI and SDI interfaces. |
| **MicroSD Card (Storage)** | SPI2 | Stores MP3 files read using FatFS. |
| **Blue User Button (PA0)** | GPIO | Controls playback gestures (Next / Previous / Shuffle). |
| **Optional 16×2 LCD (HD44780)** | GPIO (4-bit) | Displays track info (future feature). |

---

## 🎵 Features

- Auto-scan `/music` folder for `.mp3` files (LFN-compatible).  
- Displays title/artist from **ID3v1 tags** if available.  
- Physical button gestures:
  - **Single click:** Next track  
  - **Double click:** Previous track  
  - **Long press:** Shuffle toggle (reshuffles playlist in-place)  
- Fallback to embedded MP3 sample if SD mount fails.  
- Retry mount logic for slow SD initialization.  
- UART debug logging for all events (115200 bps default).


---

## 🧾 License

This project is open-source for educational and reference use under the **MIT License**.  
© 2025 [Brian Kim (kibeom12901)](https://github.com/kibeom12901)

---

**Developed with STM32CubeIDE · HAL Drivers · FatFS**
