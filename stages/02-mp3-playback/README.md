# 02 – MP3 Playback (VS10xx + Embedded Sample)

This stage demonstrates **full MP3 audio playback** on the VS1003B/VS1053 codec using an **embedded sample buffer (`MP3Sample.c/.h`)** stored in STM32 Flash.  

Unlike [01-Connection Test](../01-connection-test), which only verified SCI register access, this step configures the codec (MODE, CLOCKF, VOL) and streams real MP3 data to the decoder via the SDI interface.

---

## 📂 File Structure

- `main.c` – bring-up + playback logic (reset, SCI setup, data streaming)  
- `MP3Sample.c / MP3Sample.h` – contains `sampleMp3[]` buffer and its length `sampleMp3_len`  
- `stm32f4xx_hal_*` – CubeMX-generated drivers (SPI, GPIO, UART, DMA, etc.)  

---

## ▶️ How it Works

1. Hardware reset of VS10xx (`RST` low pulse).  
2. SCI initialization:  
   - `SM_SDINEW` enabled in `SCI_MODE`  
   - `SCI_CLOCKF` set (`0x8800`) to raise internal clock  
   - `SCI_VOL` set (`0x2020`) for moderate volume  
3. Embedded MP3 sample streamed in **≤32 byte chunks** when `DREQ` is high.  
4. Decoder flushed with zero padding (`VS_SendZeros()`).  
5. “Playback complete.” printed via UART.  

---

## 📊 Results (UART Log)

<img width="283" height="111" alt="Screenshot 2025-10-02 at 11 18 48 AM" src="https://github.com/user-attachments/assets/d0f5d2cf-33e9-426b-9305-efcbee014362" />

---

## ⚠️ Memory Limitation

Currently, the MP3 data is linked directly into RAM from `MP3Sample.c`.  

<img width="303" height="87" alt="Screenshot 2025-10-02 at 11 12 14 AM" src="https://github.com/user-attachments/assets/b63b1e38-d838-4edb-84a6-b41e2fb8a65e" />

- STM32F407 RAM = **128 KB total** (≈126 KB free at startup).  
- Even a short MP3 file (100–200 KB) can **overflow available RAM**.  

This makes the “embedded array” approach impractical for anything beyond tiny clips.  
To support full songs, the data must be stored externally.  

---

## 🔜 Next Step

To overcome the memory limit, the next stage will add:  

- **MicroSD card via SPI** (FATFS file system)  
- Stream MP3 files directly from SD → VS10xx, instead of embedding in Flash.  

This allows playback of arbitrarily long tracks without Flash constraints.  
