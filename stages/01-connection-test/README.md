# 01 – Connection Test (VS1003 SPI Bring-up)

<img width="278" height="291" alt="Screenshot 2025-10-22 at 9 43 33 AM" src="https://github.com/user-attachments/assets/c0d4fd48-244b-459d-9f52-00d580d08a36" />

This stage verifies **SPI + UART communication** with the VS1003/VS1053 audio codec by reading back its SCI registers after hardware reset.

## 🔌 Wiring (CubeMX pinout)

| VS10xx Pin | STM32F407 Pin | Notes                    |
|------------|---------------|--------------------------|
| **XCS**    | PA4           | SCI chip select (idle H) |
| **XDCS**   | PE3           | SDI chip select (unused) |
| **RST**    | PE2           | Reset (active LOW)       |
| **DREQ**   | PB0           | Data request (input)     |
| **SPI1**   | PA5/PA6/PA7   | SCK / MISO / MOSI, Mode 0|
| **UART2**  | PA2 (TX)      | 115200 baud for logs     |

## ▶️ Results

During bring-up, the SCI registers were read successfully:

<img width="514" height="71" alt="vs1003_sci_read" src="https://github.com/user-attachments/assets/b45b6ded-84f0-4429-b13e-6f2c027d2456" />
