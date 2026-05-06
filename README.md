# Dokidoki AI Heartbeat Detector

A playful heartbeat detection device using an mmWave radar module, with OLED/LCD display, LED feedback, and AI-based “dokidoki” judgment.

<img width="557" height="443" alt="dokidoki_detector" src="https://github.com/user-attachments/assets/b38fd5c5-6249-427b-9d59-0adfc1cd17da" />

## Features
- Heart rate and breathing rate measurement using an mmWave radar sensor
- LED blinking synchronized with heartbeat
- OLED/LCD display for real-time feedback
- Button-triggered 10-second data transmission
- AI-generated “dokidoki” judgment message

## Hardware
- [XIAO 60GHz mmWave Human Breathing and Heartbeat Sensor -MR60BHA2](https://jp.seeedstudio.com/MR60BHA2-60GHz-mmWave-Sensor-Breathing-and-Heartbeat-Module-p-5945.html)
- OLED/LCD display
- LED / button / switch
- DC-DC converter

## Software
- FreeRTOS-based task structure
- Measurement task
- Display task
- LED control task (heartbeat sync)
- Data buffer and FreeRTOS queue

## Dependencies
- Seeed Arduino mmWave Library
- Adafruit GFX / SSD1306
- ESP32 Arduino Core (WiFi, HTTPClient)
- FreeRTOS (built-in)

Each library is subject to its own license.

## License
MIT
