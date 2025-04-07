# 🌪️ ESP32-Based Disaster Management Audio Alert System

This project is a **disaster warning system** built using ESP32 microcontrollers. It enables government authorities to wirelessly **transmit audio alerts** (via nRF24L01 modules) to people during emergency situations such as natural disasters.

## 📡 Project Overview

- 🧠 **Transmitter ESP32**: Connects to a Wi-Fi hotspot, receives MP3 files from a web UI, converts them to RAW format using a Node.js server, and transmits the audio via nRF24L01.
- 🎧 **Receiver ESP32**: Listens for audio data over nRF24L01, stores the data in LittleFS, and plays it using a MAX98357 I2S DAC amplifier.

---

## 🔌 Hardware Used

### Transmitter Node:
- ESP32 Dev Board
- nRF24L01+ PA+LNA Module
- Wi-Fi Hotspot (e.g., mobile)
- Node.js-capable laptop for audio conversion

### Receiver Node:
- ESP32 Dev Board
- nRF24L01+ Module
- MAX98357 I2S DAC Amplifier
- Speaker

---

## 🪛 Pin Configuration

### Transmitter (nRF24L01):
- CE: GPIO 4  
- CSN: GPIO 5  
- SPI:  
  - MOSI: GPIO 23  
  - MISO: GPIO 19  
  - SCK:  GPIO 18  

### Receiver:
- Same SPI configuration as above  
- I2S (MAX98357):  
  - BCLK: GPIO 26  
  - LRCK: GPIO 25  
  - DIN:  GPIO 22  

---

## 🖼️ Web Interface

The transmitter ESP32 hosts a web UI that allows users to upload `.mp3` files.

### ⚠️ Upload Instructions
1. Compile and build the React-based frontend using Vite.
2. Upload the resulting `dist/` folder to the **transmitter ESP32** using [LittleFS Data Upload Tool](https://github.com/me-no-dev/arduino-esp32fs-plugin).

---

## 🧩 MP3 to RAW Conversion (Local Server)

### 🛠️ Node.js + FFmpeg Setup

This project includes a local server that uses **FFmpeg** via **Node.js + Express** to convert uploaded MP3 files to `.raw` format for playback on the receiver.

### 📁 Location in Repo

The code for the converter is located in the `converter/` directory of this project:
```
converter/
├── index.js
├── package.json
├── package-lock.json
└── uploads/
```

### 🚀 How to Run

1. Open a terminal inside the `converter/` directory.
2. Install dependencies:
```
npm install
```
3. Start the server:
```
node index.js
```

The server will listen on `http://<your-laptop-ip>:3000/convert`.

🔧 **Important:**  
Update the ESP32 transmitter code with your laptop’s IP address to ensure it can POST the MP3 to your machine.

---

## 🧪 How It Works

1. User uploads `.mp3` file via the ESP32-hosted webpage.
2. The file is POSTed to the Node.js server running on your laptop.
3. The server converts it using FFmpeg and returns `.raw` audio data.
4. ESP32 saves it to LittleFS as `audio.raw`.
5. Audio is sent chunk-by-chunk (32 bytes) over nRF24L01 to receivers.
6. Receivers store the file and play it via I2S DAC to a speaker.

---

## 📂 File Structure

```
project-root/
├── converter/         # Node.js + FFmpeg converter
├── ui/                # Web UI (build with Vite)
├── data/              # LittleFS image folder
├── transmitter.ino    # Main ESP32 transmitter firmware
└── receiver.ino       # Main ESP32 receiver firmware
```

---

## 📦 Dependencies

- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [nRF24L01 Radio Library](https://github.com/nRF24/RF24)
- [LittleFS](https://github.com/me-no-dev/arduino-esp32fs-plugin)
- [MAX98357 I2S Audio DAC](https://www.adafruit.com/product/3006)

---

## 📢 Contribution

Pull requests and improvements welcome! If you’re looking to extend this for other forms of alert or with different communication modules (LoRa, WiFi Mesh, etc.), go for it!

---

## 🛡️ License

MIT License
