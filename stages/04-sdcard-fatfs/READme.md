# Stage 04 — microSD (SPI2) + FatFS

Mount and read files from a microSD card using **FatFS** over **SPI2** on STM32F407.  
This stage builds on _03-sdcard-connection-test_ (CMD0/CMD8/ACMD41/CMD58 passed) and adds the file system layer.

---

## 🎯 Goals
- Keep **VS1003B** on **SPI1** (unchanged).
- Use **SPI2** for the microSD card.
- Enable **FatFS (User-defined / SPI)** and verify:
  - `f_mount()` OK
  - `f_opendir("/")` + `f_readdir()` list entries
  - Create/Write/Read/Append a small file (`test.txt`)
  - Clean unmount

---

## ⚙️ CubeMX Configuration (key points)

**FatFS**
- Interface: **User-defined**
- Generated files:
  - `Core/Src/fatfs.c`, `Core/Inc/fatfs.h`
  - **Your** low-level driver: `Core/Src/user_diskio.c`, `Core/Inc/user_diskio.h`

**FatFS (ffconf.h) tips**
- `FF_MIN_SS = 512`, `FF_MAX_SS = 512`
- `FF_FS_READONLY = 0`
- `FF_USE_LFN` as needed (0/1)
- exFAT off unless you need it

---

## 🧩 user_diskio.c (User-defined SPI block I/O)

Implement these over SPI2 with your CS macros:
- `USER_initialize()`
- `USER_status()`
- `USER_read(BYTE* buff, LBA_t sector, UINT count)`
- `USER_write(const BYTE* buff, LBA_t sector, UINT count)` *(if write enabled)*
- `USER_ioctl(BYTE cmd, void* buff)` → handle `CTRL_SYNC`, `GET_SECTOR_COUNT`, `GET_BLOCK_SIZE`, etc.

**Notes**
- Ensure 512-byte sector size.
- Assert CS low for command/transfer, deassert high after.
- Provide enough dummy clocks (0xFF) when required.

---

## 🧪 Test Program Flow (main.c)

1. **SPI SD quick check**  
   - 80 dummy clocks (CS=HIGH)  
   - `CMD0` → idle  
   - `CMD8(0x1AA)` → SDv2 check  
   - Loop `ACMD41` (HCS if v2) until ready  
   - `CMD58` → read OCR, parse CCS (SDHC/SDXC)

2. **FatFS**
   - `f_mount(&USERFatFS, USERPath, 1)`
   - Create/Write `test.txt`
   - Read back `test.txt`
   - Append `"Appended line.\r\n"`
   - `f_opendir("/")` + `f_readdir()` to list entries
   - `f_mount(NULL, USERPath, 1)` to unmount cleanly

**Append = write at end of file** (does not overwrite previous content).

---

## RESULTS

<img width="402" height="197" alt="Screenshot 2025-10-16 at 1 54 16 PM" src="https://github.com/user-attachments/assets/38eddada-656d-4d6c-8fb3-b693f6a9cf70" />


