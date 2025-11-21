# 🔌 Pinout — VS1003B MP3 Player (STM32F407)

All signals are **3.3 V logic**. Grounds of all modules must be common.

---

## 1) VS1003B ↔ STM32F407 (SPI1)

| Function | VS1003B module pin* | STM32F407 pin | HAL/Cube name |
|---|---|---|---|
| SPI SCK  | `SCK` / `SCLK`       | **PA5**       | `SPI1_SCK` |
| SPI MISO | `MISO` / `SO`        | **PA6**       | `SPI1_MISO` |
| SPI MOSI | `MOSI` / `SI`        | **PA7**       | `SPI1_MOSI` |
| SCI CS   | `XCS`                | **PA4**       | GPIO out (CS for control registers) |
| SDI CS   | `XDCS`               | **PE3**       | GPIO out (CS for data stream) |
| Reset    | `XRST` / `RST`       | **PE2**       | GPIO out |
| DREQ     | `DREQ`               | **PB0**       | GPIO in (floating) |
| 3.3 V    | `VCC`                | **3V3**       | — |
| GND      | `GND`                | **GND**       | — |
| Audio L  | `LOUT`               | —             | to jack/amp via AC-coupling (per module) |
| Audio R  | `ROUT`               | —             | to jack/amp via AC-coupling (per module) |

\* Pin names vary slightly by breakout; use the equivalents shown.

**Notes**
- `XCS` is the **SCI** (register) chip select; `XDCS` is the **SDI** (stream) chip select.
- Keep SCK/MOSI/MISO short and routed away from the analog audio lines.

---

## 2) microSD Adapter ↔ STM32F407 (SPI2)

| Function | microSD (SPI mode) | STM32F407 pin | HAL/Cube name |
|---|---|---|---|
| SPI SCK  | `CLK`               | **PB13**      | `SPI2_SCK` |
| SPI MISO | `DO`                | **PB14**      | `SPI2_MISO` |
| SPI MOSI | `DI`                | **PB15**      | `SPI2_MOSI` |
| Chip-Select | `CS`             | **PB12**      | GPIO out (SD CS) |
| 3.3 V    | `VCC`               | **3V3**       | — |
| GND      | `GND`               | **GND**       | — |

**Notes**
- No level shifting is required if your SD breakout is 3.3 V.  
- Add a 100 nF decoupling cap near the module if not already present.

---

## 3) Controls & Debug

### User Button (track control / shuffle)
| Signal | Board label | STM32F407 pin | HAL/Cube name |
|---|---|---|---|
| Button | `USER`      | **PA0**       | `GPIO_EXTI0` (polled in firmware) |

**Wiring**: Use the on-board USER button (PA0 on Discovery), or an external momentary to PA0 with **GND** and internal pull-down (configured in Cube).

### Volume Potentiometer (analog)
| Function | Net | STM32F407 pin | HAL/Cube name |
|---|---|---|---|
| Wiper   | → ADC | **PA1**       | `ADC1_IN1` |
| Side A  | 3.3 V | **3V3**       | — |
| Side B  | GND   | **GND**       | — |

**Notes**: 10 kΩ linear pot recommended. Firmware maps 0..4095 → VS1003 `SCI_VOL`.

### UART Logging
| Function | STM32F407 pin | HAL/Cube name | To |
|---|---|---|---|
| TX | **PA2** | `USART2_TX` | USB-UART RX |
| RX | **PA3** | `USART2_RX` | USB-UART TX |
| GND | **GND** | — | USB-UART GND |

Baud: **115200-8-N-1** (as used in examples).

---

## 4) Power

| Rail | Connects to |
|---|---|
| **3.3 V** | STM32F4 3V3, VS1003B VCC, microSD VCC, potentiometer high side |
| **GND**   | Common ground to all modules and USB-UART |

**Noise tips**
- Place **0.1 µF** caps near VS1003 and SD VCC.  
- Keep audio lines short; route away from SPI lines.

---

## 5) Quick Reference (Firmware Macros)

These match the default firmware defines:

```c
// VS10xx control pins
#define VS_XCS  PA4   // SCI CS
#define VS_XDCS PE3   // SDI CS
#define VS_RST  PE2   // Reset
#define VS_DREQ PB0   // Data request

// SD card (SPI2) CS
#define SD_CS   PB12  // SD CS

// Controls
#define USER_BTN PA0  // Button (EXTI0)
#define VOL_ADC  PA1  // ADC1_IN1

// SPI busses
// VS1003  -> SPI1: PA5/PA6/PA7
// microSD -> SPI2: PB13/PB14/PB15
