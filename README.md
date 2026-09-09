# TRAPNET: Edge-AI Physical Intrusion Response System

[![IEEE Format](https://img.shields.io/badge/IEEE-Research_Paper-blue.svg)](TRAPNET_IEEE_Research_Paper_Final_v4.docx)
[![ESP32 Firmware](https://img.shields.io/badge/ESP32-Arduino_IDE-green.svg)](TRAPNET_ESP32_Firmware/TRAPNET_ESP32_Firmware.ino)
[![YOLOv8 Edge AI](https://img.shields.io/badge/Mobile_Edge_AI-YOLOv8_INT8-orange.svg)](TRAPNET_Master_Learning_Guide.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

An advanced, multi-modal **Physical Intrusion Detection & Response System** combining **24GHz mmWave Radar**, **YOLOv8 Mobile Edge Vision**, and **Autonomous Active Interception (Solenoid Door Locking & Non-Lethal Fog Generation)**.

---

## 🌟 Key Features

* **Multi-Modal Radar-Vision Fusion**: Cross-verifies 24GHz FMCW mmWave radar target range ($D_{\text{radar}}$) against smartphone camera optical range ($D_{\text{vision}}$) to achieve zero false alarms.
* **Sub-15ms Mobile Edge AI**: Replaces traditional Raspberry Pi nodes with an Android NPU pipeline running **YOLOv8 INT8 quantized models** via TensorFlow Lite / NNAPI.
* **Autonomous Active Response**: Instant trigger of active containment mechanisms (High-speed Solenoid Locks & Non-Lethal Fog Generators) upon confirmed human threat detection.
* **Resilient Dual Telemetry**: High-bandwidth 5G/Wi-Fi cloud snapshot uploads backed up by fallback **SIM800L GSM SMS telemetry** and local MicroSD offline logging.
* **Uninterruptible Power Supply (UPS)**: Built-in 18650 Li-ion battery UPS system with LM2596 high-efficiency DC-DC buck converters.

---

## 📐 System Architecture

```
                       +-------------------------------------------------------+
                       |              MOBILE PHONE EDGE NODE                   |
                       |  (Android Foreground Service + Camera2 API)           |
                       |                                                       |
                       |  1. Camera Frames (1080p @ 60 FPS)                     |
                       |  2. YOLOv8n TFLite/NNAPI Inference Engine (8-15 ms)   |
                       |  3. Radar-Vision Cross-Verification Logic            |
                       |  4. Cloud Snapshot Upload (5G/Wi-Fi) + Local GPS      |
                       +---------------------------+---------------------------+
                                                   |
                                 USB-OTG Serial / BLE 5.0 / Wi-Fi Socket
                                 Bidirectional Protocol (@ 115200 Baud)
                                                   |
                       +---------------------------v---------------------------+
                       |               ESP32 MASTER CONTROLLER                 |
                       |                                                       |
                       |  1. mmWave Radar (HLK-LD2410C) continuous tracking    |
                       |  2. IR Beam & SW-420 Vibration Interrupts             |
                       |  3. Relay Triggers (Solenoid Lock & Fog Generator)    |
                       |  4. SIM800L GSM Fallback SMS Telemetry                |
                       +-------------------------------------------------------+
```

---

## 🔌 ESP32 Master Hardware Pinout Mapping

| Peripheral / Module | Protocol / Interface | ESP32 Pin Assignment | Voltage / Specs |
| :--- | :--- | :--- | :--- |
| **HLK-LD2410C mmWave Radar** | Hardware Serial 2 | `GPIO16` (RX2) / `GPIO17` (TX2) | 5V / 256,000 baud |
| **SIM800L GSM Module** | SoftwareSerial 1 | `GPIO26` (RX) / `GPIO27` (TX) | 4.1V Dedicated Buck / 9,600 baud |
| **Android Phone Link** | Primary USB Serial 0 | `GPIO3` (RX0) / `GPIO1` (TX0) | 5V USB-OTG / 115,200 baud |
| **Relay 1 (Solenoid Lock)** | Digital Output | `GPIO32` | Active-LOW Trigger |
| **Relay 2 (Fog Generator)** | Digital Output | `GPIO33` | Active-LOW Trigger |
| **Door IR Sensor** | Digital Input | `GPIO4` | `INPUT_PULLUP` |
| **Window IR Sensor** | Digital Input | `GPIO13` | `INPUT_PULLUP` |
| **SW-420 Vibration #1** | Digital Input | `GPIO34` | Input-Only ($10\text{k}\Omega$ pull-down) |
| **SW-420 Vibration #2** | Digital Input | `GPIO39` | Input-Only ($10\text{k}\Omega$ pull-down) |
| **0.96" SSD1306 OLED** | I2C Bus | `GPIO21` (SDA) / `GPIO22` (SCL) | 3.3V / 400kHz |
| **MicroSD Module** | VSPI Bus | `GPIO18` (SCK), `GPIO19` (MISO), `GPIO23` (MOSI), `GPIO5` (CS) | 5V / SPI Mode |

---

## 📄 Repository Deliverables

* 📁 `TRAPNET_ESP32_Firmware/`: Complete C++ firmware sketch runnable on **Arduino IDE 2.3+**.
* 🌐 `TRAPNET_Pin_Connections_Explanation.html`: Interactive hardware wiring schematic & filterable pin connection guide.
* 🌐 `TRAPNET_Master_Learning_Guide.html`: Interactive study guide covering Embedded Systems, Edge AI, and Power Electronics.
* 📄 `TRAPNET_IEEE_Research_Paper_Final_v4.docx`: Complete publication-ready IEEE format research paper.

---

## ⚡ Quick Start (Arduino IDE Setup)

1. Open **Arduino IDE 2.x**.
2. Go to **File** $\rightarrow$ **Preferences** and add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json` to Boards Manager URLs.
3. Install the **esp32** board package and select **ESP32 Dev Module**.
4. Open [`TRAPNET_ESP32_Firmware/TRAPNET_ESP32_Firmware.ino`](TRAPNET_ESP32_Firmware/TRAPNET_ESP32_Firmware.ino).
5. Click **Upload** to flash the ESP32 microcontroller over USB.

---

## 👨‍💻 Author & Maintainer

**Mohamed Sameer**  
GitHub: [@MohamedSameer-dev](https://github.com/MohamedSameer-dev)
