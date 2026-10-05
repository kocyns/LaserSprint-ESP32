# ⚡ Laser Sprint – ESP32-C3 & TF-Luna LiDAR Timing Gate

**Laser Sprint** is an open-source, ultra-low latency wireless photogate timing system designed for track & field, agility testing, and speed profiling. Powered by an ESP32-C3 microcontroller and a TF-Luna Time-of-Flight (ToF) LiDAR sensor, it calculates instant split times and transmits data directly to Garmin smartwatches via Bluetooth Low Energy (BLE).

---

## 🌟 Key Features
- **Garmin Smartwatch Integration:** Direct real-time split times and velocity tracking on your wrist without needing a smartphone.
- **High-Frequency ToF LiDAR Sampling:** Uses TF-Luna LiDAR at 100 Hz with adaptive baseline distance evaluation for sub-millisecond trigger precision.
- **Custom BLE Protocol (`LS_GATE`):** Dynamic 128-bit UUID broadcasting time offset, gate ID, and battery levels.
- **Smart Power Management:** Deep Sleep mode with 2-second hold button wake-up/shutdown and automated 5V power gating.
- **LiPo & USB-C Powered:** Onboard battery level monitoring via ADC with NeoPixel LED status feedback.

---

## 🛠️ Hardware Specification & Pinout

The custom PCB uses the **ESP32-C3** module with the following pin assignments:

| Function | ESP32-C3 Pin | Description |
| :--- | :--- | :--- |
| **TF-Luna RX** | GPIO 5 | UART receive line (115200 baud) |
| **TF-Luna TX** | GPIO 4 | UART transmit line |
| **NeoPixel LED** | GPIO 0 | WS2812B RGB status indicator |
| **Power Button** | GPIO 1 | Input with internal pull-up & deep sleep wake-up |
| **Battery ADC** | GPIO 3 | Voltage divider input for LiPo level detection |
| **5V Power Gate** | GPIO 6 | High-side power control for peripheral rail |

> Manufacturing files (Gerber files, PCBWay project link) are available in the `/hardware` folder.

---

## 🚀 Getting Started

### 1. Hardware Assembly
1. Fabricate the PCB using the provided Gerber files in `/hardware`.
2. Solder the ESP32-C3, TF-Luna LiDAR sensor, NeoPixel LED, and power components.
3. 3D print the enclosure files from `/3d_models`.

### 2. Flashing the Firmware
1. Open the project in **Arduino IDE** or **PlatformIO**.
2. Install the required dependency:
   - `Adafruit_NeoPixel` library
3. Select board: **ESP32C3 Dev Module**.
4. Compile and flash the code located in [V2_1_4.ino](firmware/V2_1_4.ino).

---

## 📡 BLE Protocol (`LS_GATE`)

The timing gate broadcasts its status using custom Service UUIDs structured as follows:
`11111111-2222-3333-4444-555555[GateID][Active][OffsetTens][BatDigit]`

- **Gate ID:** Derived automatically from the last byte of the ESP32 MAC address.
- **Active State:** `1` when a trigger event is detected, `0` when idle.
- **Offset Tens:** Time elapsed since trigger detection (0–99 tens of milliseconds).
- **Bat Digit:** Battery level digitized from `0` (empty) to `9` (full).

---

## ⌚ Garmin App Compatibility
Download the companion **Laser Sprint** application on the Garmin Connect IQ Store to pair with one or multiple timing gates.

---

## 📄 License
This project is licensed under the **CC BY-NC-SA 4.0** (Creative Commons Attribution-NonCommercial-ShareAlike).
