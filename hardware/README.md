# ⚙️ Hardware Overview — VS1003B MP3 Player (STM32F4)

This directory documents the **hardware setup and wiring** used for the VS1003B MP3 Player project.  
It includes pin connections, bill of materials (BOM), and wiring references for reproducibility.

---

## 📸 Setup Overview

Below is the physical prototype built on a breadboard using the **STM32F407 Discovery** board.  
The system integrates the VS1003B MP3 decoder, microSD SPI module, potentiometer (volume control), user button, and UART serial logging.

<img width="855" alt="Hardware Setup" src="https://github.com/user-attachments/assets/c728d89c-05e6-4238-be99-79cdf7487652" />

**Labeled Components:**
- 🟩 **STM32F407** — Main MCU for SPI/ADC control  
- 🔊 **VS1003 MP3 Decoder** — SPI1 interface for audio decoding  
- 💾 **Micro SD Card Adapter** — SPI2 interface for FATFS storage  
- 🎚️ **Potentiometer** — Analog input (PA1, ADC1_IN1) for volume  
- 🔘 **User Button** — PA0 (EXTI0) for track control and shuffle mode  
- 🔌 **UART2** — Serial log output for debugging (PA2/PA3)  
- 💽 **USB SD Card Reader** — For loading MP3 files to SD card  

---

## 🔌 Pin Connections

See [`pinout.md`](./pinout.md) for the full wiring table between:
- STM32F407 ↔ VS1003B Codec (SPI1)
- STM32F407 ↔ microSD Card Adapter (SPI2)
- STM32F407 ↔ Button (EXTI0) and Potentiometer (ADC1_IN1)
- UART2 for serial output (PA2 / PA3)

> ⚠️ **All modules operate at 3.3 V logic.**  
> Do *not* connect 5 V modules directly to STM32 pins.

---

## 🧾 Bill of Materials (BOM)

The complete parts list is provided in [`bom.csv`](./bom.csv), including:
- STM32F407 Discovery board  
- VS1003B MP3 decoder module  
- microSD SPI breakout module  
- 10 kΩ potentiometer for analog volume input  
- User button for playback control  
- Breadboard and jumper wires  
- USB–UART adapter for serial logging  
- USB SD card reader (for file transfer)  

---

## ⚙️ Power and Noise Notes

- Supply all peripherals with **3.3 V**.  
- Place **0.1 µF decoupling capacitors** close to VS1003B and SD power pins.  
- Keep analog audio traces short and separate from SPI lines.  
- Optionally add **AC coupling capacitors (1 µF)** at VS1003B audio outputs for cleaner sound.  
- Ensure common ground between all modules.  

---

## 📂 Files in This Folder

| File | Description |
|------|--------------|
| [`pinout.md`](./pinout.md) | MCU ↔ peripheral pin mapping table. |
| [`bom.csv`](./bom.csv) | Bill of materials for all components. |
| `hardware-setup.png` | Annotated photo of the real hardware setup. |

---

## 🧩 Related Project Structure

- Firmware stages under [`../stages`](../stages)  
- Modularized driver libraries under [`../lib`](../lib)  

---

**Author:** Brian Kim  
**Project:** VS1003B MP3 Player — STM32F4 HAL-Based Driver Demo  
**License:** MIT
