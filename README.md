# Resilient Edge IoT Node with Fault-Tolerant Data Buffering

A production-grade, fault-tolerant ESP32 system designed to eliminate telemetry data loss during network dropouts. This project implements a structural tri-state health monitoring machine (`ONLINE`, `DEGRADED`, `OFFLINE`) and handles cloud synchronization using an intelligent exponential backoff mechanism.

## 🚀 Features
* **Zero Data Loss:** Local FIFO RAM queue buffering caches up to 50 historical telemetry frames during Wi-Fi or MQTT broker disconnects.
* **Intelligent Network Re-entry:** Implements an exponential backoff recovery algorithm to prevent network flooding upon reconnection.
* **Local UI & Diagnostics:** Real-time system monitoring via an SSD1306 OLED display, visual tri-color LED health indicators, and distinct frequency buzzer alarms.
* **Cloud Integration:** Real-time data visualization streams seamlessly to Adafruit IO.

## 🛠️ Hardware & Components (Wokwi Simulation)
* **Microcontroller:** ESP32 Development Board
* **Sensors:** DHT22 (Temperature & Humidity)
* **Display & Indicators:** SSD1306 OLED ($128 \times 64$ I2C), 3x Status LEDs (Green, Yellow, Red), Piezo Buzzer

## 📂 Repository Structure
* `/firmware`: Contains the `sketch.ino` C++ source code.
* `diagram.json`: The physical layout schematics for the Wokwi simulator canvas.
* `libraries.txt`: Required dependencies for compilation.

## 📈 Cloud Dashboard Configuration
* **Temperature Feed:** Monitored via a Gauge Block (Range: 0–50°C).
* **Humidity Feed:** Tracked via a Line Chart Block (Fixed Y-Axis: 0–100% RH) to visualize data recovery bursts.
