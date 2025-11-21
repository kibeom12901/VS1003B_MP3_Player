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

<img width="633" height="374" alt="Screenshot 2025-11-21 at 10 41 24 AM" src="https://github.com/user-attachments/assets/082f22d0-2315-4d6f-a61c-3c51ae907b7a" />

| Component | Interface | Description |
|:--|:--|:--|
| **VS1003B MP3 Decoder** | SPI1 (PA5/PA6/PA7) + GPIO (DREQ, XRST, XCS, XDCS) | Handles MP3 decoding via SCI/SDI. Audio output from VS1003B. |
| **MicroSD Card Module** | SPI2 (PB13/PB14/PB15) | Stores MP3 files; accessed using FatFS. |
| **Potentiometer (10kΩ)** | ADC1 (PA1 – ADC1_IN1) | Real-time volume control. |
| **User Button** | GPIO + EXTI0 (PA0) | Playback gestures (Next / Previous / Shuffle). |
| **UART2 (PA2/PA3)** | USART2 | Serial logging for debugging. |
| **STM32F407 Discovery Board** | — | Main MCU handling SPI, ADC, EXTI, decoding pipeline. |

More detailed pin mapping, BOM, wiring diagrams, and annotated hardware photos can be found in **[`hardware/`](./hardware)**

This directory includes:

- `pinout.md` — Complete MCU ↔ peripheral wiring table  
- `bom.csv` — Full bill of materials  
- `hardware-setup.png` — Annotated breadboard setup  
- `README.md` — Detailed hardware overview and wiring notes  
---
## 🚀 Quick Start

### 1. Clone the repository

```bash
git clone https://github.com/kibeom12901/VS1003B_MP3_Player.git
```

### 2. Open with STM32CubeIDE

- Tested on **STM32CubeIDE 1.xx.x**
- Open the project inside any of the `/stages` folders
- Or use the final version: `/stages/07-FINAL-modularized-lib`

### 3. Build & Flash

- Connect the **STM32F407 Discovery** board
- Click **Build → Run**

### 4. Prepare SD Card

- Create a folder: `/music/`
- Copy `.mp3` files (supports LFN)

### 5. Run

- Reset the board
- Music should play automatically

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
