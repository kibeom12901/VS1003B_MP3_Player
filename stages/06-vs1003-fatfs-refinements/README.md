# Stage 06 — VS1003 + FatFS Refinements

Enhancements to the SD/FatFS MP3 player:

- Added retry logic for SD mount  
- Implemented single, double, and long press gestures on the PA0 user button  
- Added shuffle mode with in-place playlist reshuffle  
- Verified stable SD communication and FatFS mounting behavior under long sessions  

---

## ✨ Feature Explanations

### 🧩 1. SD Mount Retry Logic
Sometimes, the SD card may fail to mount on the first attempt—especially after power-up or when the card initialization takes longer.  
To address this, the firmware now performs **automatic retries**:
- Attempts `f_mount()` up to **3 times** with short delays between each.
- Each retry re-initializes the **SPI2 interface** and re-asserts the **CS** line.
- On success, playback proceeds normally; on repeated failure, the system falls back to the internal sample track.

<img width="401" height="199" alt="Screenshot 2025-10-22 at 9 21 23 AM" src="https://github.com/user-attachments/assets/c40f0935-90bc-4356-ad2f-75e61aafd500" />

---

### 🎛️ 2. Gesture Control on PA0 (User Button)
The PA0 (Blue USER Button) now recognizes **three distinct gestures** using EXTI0 interrupts and a TIM7-based timer:

| Gesture | Action |
|----------|--------|
| **Single Click** | Next Track |
| **Double Click** | Previous Track |
| **Long Press (> 1 s)** | Toggle Shuffle Mode |

Debounce and timing thresholds were tuned to ensure consistent detection without false triggers, even during long presses.

### Single & Double click

<img width="457" height="135" alt="Screenshot 2025-10-22 at 9 24 04 AM" src="https://github.com/user-attachments/assets/7244d866-6e6d-4ee5-962b-e09d0d4a8d47" />

---

### 🔀 3. Shuffle Mode with In-Place Reshuffle
When shuffle is toggled **ON**, the firmware randomizes the playlist order using the **Fisher–Yates algorithm**, ensuring each track appears exactly once before repeating.  
Turning shuffle **OFF** restores sequential playback starting from index 0.  
This maintains consistent indexing and smooth transitions between tracks.

<img width="437" height="119" alt="Screenshot 2025-10-22 at 9 24 43 AM" src="https://github.com/user-attachments/assets/76a809c4-32ac-473f-b42a-3c3a94270d48" />

---

### ⚙️ 4. ADC Test (Preparation for Volume Control)

<img width="536" height="377" alt="Screenshot 2025-10-21 at 2 16 48 PM" src="https://github.com/user-attachments/assets/15e1fcf4-92ff-46ca-8822-97f920ee55a1" />

Before integrating analog volume control, the ADC input was validated using a potentiometer connected to **PA1 (ADC1_IN1)**.

#### Configuration

| Component | Setting |
|------------|----------|
| **Pin** | PA1 (ADC1_IN1) |
| **Mode** | Analog mode (No pull-up/pull-down) |
| **ADC Mode** | Single conversion, software-triggered |
| **Sampling Time** | 144 cycles |
| **Polling Method** | HAL_ADC_Start / HAL_ADC_PollForConversion |
| **Output** | Live UART print (USART2 @ 115200 baud) |

#### Test Output
After flashing the firmware, turning the potentiometer produced live UART output:

<img width="182" height="168" alt="Screenshot 2025-10-21 at 2 17 57 PM" src="https://github.com/user-attachments/assets/7dea0294-ed0e-4bda-b2cf-117876d952d4" />

#### Real Implementation

<img width="274" height="126" alt="Screenshot 2025-10-22 at 9 17 50 AM" src="https://github.com/user-attachments/assets/f7893d08-3051-4709-a5c0-4b55fc8e0598" />

