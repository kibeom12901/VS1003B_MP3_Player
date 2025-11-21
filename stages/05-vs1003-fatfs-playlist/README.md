# 🎵 Stage 05 — VS1003 + FatFS MP3 Playlist Player

This stage combines **VS1003 MP3 decoding**, **FatFS file streaming**, and a **hardware skip button** into a fully functional SD-based music player on the **STM32F407**.

---

## 🚀 Overview

This stage builds upon the previous modules:

| Stage | Description |
|:------|:-------------|
| **01-connection-test** | Basic GPIO & UART connectivity check |
| **02-mp3-playback** | Play embedded MP3 sample from flash (no SD) |
| **03-sdcard-connection-test** | SPI2 SD card low-level communication test (CMD0 / ACMD41) |
| **04-sdcard-fatfs** | Mount and read files using FatFS over SPI2 |
| **➡️ 05-vs1003-fatfs-playlist** | Stream real MP3 files from SD to VS1003 with playlist + skip button |

---

## 🧩 Features

- 🎧 **VS1003B MP3 Decoder** on SPI1  
  - SCI register setup (`SCI_MODE`, `SCI_CLOCKF`, `SCI_VOL`)  
  - DREQ-driven streaming (up to 512 B chunks)
- 💾 **microSD (SPI2 + FatFS)**  
  - Mounts `/` and streams MP3 files using FatFS `f_read()`
- 🎶 **Playlist Support**  
  - Automatically cycles through `/music/track1.mp3`, `/track2.mp3`, `/track3.mp3`
- 🔘 **Blue USER Button (PA0 / EXTI0)**  
  - Skips to the next track instantly (debounced)
- 🧠 **Fallback Mode**  
  - If SD card mount or file open fails → plays embedded `MP3Sample.h`

---

## 💽 Preparing the SD Card

1. Download `.mp3` files from [**mp3juices.click**](https://v3.mp3juices.click/)  
   (used for testing purposes only).

2. Create a folder named **`/music`** in the root directory of your microSD card.

3. Copy your downloaded MP3 files into that folder using a **USB 3.0 SD card adapter** connected to your **Mac**.

    <img width="375" height="212" alt="Screenshot 2025-10-22 at 9 54 40 AM" src="https://github.com/user-attachments/assets/e72143be-4be8-4b1c-bd4a-467ec73bae9e" />

4. Eject the SD card safely and insert it into your STM32 setup.

> ⚠️ Make sure the SD card is formatted as **FAT32**, otherwise FatFS mounting will fail.

<img width="455" height="194" alt="Screenshot 2025-10-20 at 1 52 13 PM" src="https://github.com/user-attachments/assets/7052120b-9800-4448-8791-de7a4d20b2b3" />

---

## 🧠 Software Flow

```text
┌────────────────────────────────────────┐
│            System Startup               │
│  HAL_Init(), Clock, GPIO, SPI, FatFS    │
└────────────────────────────────────────┘
             │
             ▼
    SD Card Quick Check (SPI2)
             │
      ┌──────┴────────┐
      │               │
      ▼               ▼
f_mount() OK      f_mount() FAIL
(use SD + FatFS)  → play embedded sample
      │
      ▼
VS1003 Initialization (SCI + mode setup)
      │
      ▼
Main Loop → PlayFile()
  - open `/music/trackN.mp3`
  - double-buffer streaming
  - check DREQ and PA0 skip flag
  - auto-next track on EOF

```

---

## 🧩 Playlist Configuration

Defined in main.c

<img width="247" height="228" alt="Screenshot 2025-10-20 at 1 21 20 PM" src="https://github.com/user-attachments/assets/6d53e756-78a8-4bec-8d2d-bed0d3aecc71" />

---

## Debug Output (UART @115200bps)

<img width="394" height="236" alt="Screenshot 2025-10-20 at 1 25 25 PM" src="https://github.com/user-attachments/assets/511cb864-f715-4683-be89-65ce3026d760" />

