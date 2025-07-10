# TinyML-Split-Model-learning-ESP32-S3
# 🤖 Distributed Machine Learning on IoT Devices with Edge Server Support

This repository demonstrates a comprehensive open source framework for executing distributed machine learning (ML) using resource-constrained IoT devices and an edge server. The system is designed to split ML model inference tasks between IoT nodes and a more powerful edge server, using various communication protocols such as Wi-Fi (TCP/UDP), ESP-NOW, and BLE.

## 🗂️ Repository Structure

### 1. `IoT nodes/`

Contains Arduino-based firmware for ESP32 microcontrollers. This directory is subdivided by the type of protocol and experiment:

#### 📁 Subfolders:

* **`TCP_UDP_ESPNOW_BLE_MEASUREMENT_TR/`**: Contains scripts for round-trip time (RTT) measurement using various communication protocols.

  * Examples:

    * `rtt_tcp_sender.ino`, `rtt_tcp_receiver.ino`
    * `esp-now-sender.ino`, `esp-now-receiver.ino`
* **`DISTRIBUTED_ML_UDP/`, `DISTRIBUTED_ML_TCP/`, `DISTRIBUTED_ML_ESP_NOW/`, `DISTRIBUTED_ML_BLE/`**: Implement distributed ML inference using different protocols.

  * Each contains two submodules:

    * `WIFI_ML1_*`: Executes the first half of the MobileNetV2 model.
    * `WIFI_ML2_*`: Sends data to the edge server for second-half inference.
  * Includes model header and C++ files, `.ino` firmware scripts, and memory partitioning configs (`partitions.csv`).

### 2. `Edge_server_scripts/`

Contains the edge server-side Python scripts and ML model files.

#### 🧠 Files and Folders:

* **Model Files:**

  * `First_Part_Model.h5`, `Second_Part_Model.h5`: Keras models for the respective halves.
  * Quantized versions: `.tflite` models for efficient inference.
* **Inference Scripts:**

  * `MobileNetV2.py`, `MobileNetV2_Split.py`: Full and split model implementations.
  * Testers like `test_Mobilenetv2_Q_split_tester.py` for accuracy and latency testing.
* **🖼️ Image Dataset:**

  * A `photos/` folder containing hundreds of dog images for benchmarking.

## 🌟 Features

* Protocol benchmarking for RTT and data transmission efficiency.
* Real-time distributed inference using model partitioning.
* Quantized and float model inference.
* Fully compatible with ESP32-S3 and similar microcontrollers.

## ⚙️ Setup Instructions

### 📋 Prerequisites

* ESP32 development board (e.g., ESP32-S3)
* Arduino IDE or PlatformIO
* Python 3.8+
* Required Python packages (install via `pip install -r requirements.txt` if available)

### 🛠️ Steps

1. **Prepare IoT Devices**

   * Open the appropriate `.ino` file (e.g., `WIFI_ML1_UDP.ino`) in the Arduino IDE.
   * Ensure required libraries and board packages are installed.
   * Upload the firmware to the ESP32 board.

2. **Set Up Edge Server**

   * Clone this repo and navigate to `Edge_server_scripts/`
   * Run a script like `MobileNetV2_Split.py` or one of the test scripts.
   * The server waits for data from the IoT node and performs the second half of model inference.

3. **Test and Observe**

   * Use sample images from the `photos/` directory or your own.
   * Measure latency, analyze output predictions, and log performance.

## 🚀 Applications

* Smart surveillance systems
* Distributed vision systems in robotics
* Low-power ML inference in remote sensing and agriculture

## 🤝 Contributing

Feel free to fork the repo, open issues, and submit pull requests.


For any queries or support, feel free to open an issue.

