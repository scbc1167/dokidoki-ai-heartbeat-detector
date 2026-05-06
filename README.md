# Dokidoki AI Heartbeat Detector

A playful heartbeat detection device using an mmWave radar sensor, with OLED/LCD display, LED feedback, and AI-based “dokidoki” judgment.

## Features
- Heart rate and breathing rate measurement using an mmWave radar sensor
- LED blinking synchronized with heartbeat
- OLED/LCD display for real-time feedback
- Button-triggered 10-second data transmission
- AI-generated “dokidoki” judgment message

## Hardware
- Seeed Studio XIAO ESP32
- mmWave radar module
- OLED/LCD display
- LED / button / switch
- DC-DC converter

## Software
- FreeRTOS-based task structure
- Measurement task
- Display task
- LED control task (heartbeat sync)
- Data buffer and FreeRTOS queue

## License
MIT
