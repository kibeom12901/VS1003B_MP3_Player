# Stage 03 — microSD (SPI2) Connection Test

<img width="394" height="334" alt="Screenshot 2025-10-22 at 9 51 04 AM" src="https://github.com/user-attachments/assets/a9b04311-7ae6-4383-9ee9-3cb070ab42bf" />

## 🎯 Objective
Test SPI2 communication and microSD card initialization before integrating FatFS.  
This stage ensures correct wiring, SPI configuration, and card readiness detection (CMD0/ACMD41 sequence).
 
---

## ⚙️ Hardware Configuration

| Peripheral | Function | STM32 Pin | Notes |
|-------------|-----------|-----------|-------|
| **SPI2** | SD Card Communication |  |  |
| ├── SCK | PB13 | SPI2_SCK (AF5) |
| ├── MISO | PB14 | SPI2_MISO (AF5) |
| ├── MOSI | PB15 | SPI2_MOSI (AF5) |
| ├── CS | PB12 | GPIO Output (manual control, idle HIGH) |
| **Power** | VCC | 3.3 V (bare board) or 5 V (adapter with regulator) |
| **GND** | Common Ground |  | — |

> ⚠️ If you use a **MicroSD module with regulator + level shifters**, power it from **5 V**.  
> For bare 3.3 V breakout boards, power from **3.3 V** only.

---

## 🧩 SPI Configuration (CubeMX)

| Parameter | Value |
|------------|--------|
| Mode | Full-Duplex Master |
| Data Size | 8 bits |
| Clock Polarity | Low (CPOL = 0) |
| Clock Phase | 1st Edge (CPHA = 0) |
| First Bit | MSB First |
| NSS | Software |
| CRC | Disabled |
| Prescaler | **/16 (~2.625 MHz)** |

✅ SPI2 is used **exclusively** for the microSD card.  
VS1003B remains on **SPI1**, so both buses can run simultaneously.

---

## 🔍 Test Procedure

1. Power the microSD module.  
2. Flash and run the firmware.  
3. Observe the UART2 (115200 bps) output.

### RESULTS
<img width="337" height="28" alt="Screenshot 2025-10-14 at 10 41 39 AM" src="https://github.com/user-attachments/assets/dee3d1ff-f5de-4f40-a624-c550c01e2115" />


### 🧠 Explanation
| Field | Meaning | Interpretation |
|--------|----------|----------------|
| **SD_VERSION=V2** | Card follows the SD Specification v2.0 or later | Indicates a **high-capacity SDHC/SDXC card** (2 GB and above). |
| **ACMD41=OK** | Card successfully responded to initialization command sequence | Confirms the card is properly powered, clocked, and communicating via SPI2. |
| **OCR=0xC0FF8000** | Operating Conditions Register value returned by the card | Bits confirm **3.2 – 3.4 V** voltage range and the card’s power-up status (bit 31 = 1). |
| **CCS=1** | Card Capacity Status bit | `1` → **SDHC/SDXC** block-addressing mode (512 B fixed block size). |

### 🟢 Conclusion
The SPI2 link and SD-card handshake are fully operational.  
The card identifies as a **V2 SDHC** device, initialization via **ACMD41** succeeded,  
and it’s now ready for **Stage 04 — FatFS Integration**.

