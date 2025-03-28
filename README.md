# <p align="center">CloudSync 📡💾</p>

<p align="center">
  <img src="https://github.com/user-attachments/assets/20132858-7061-4611-893e-fcbb4551bf07" alt="CloudSync Logo" width="200">
</p>

**CloudSync** is a cloud-based network storage solution (basically a NAS) for the ESP32, made by **c1ph3r-1337**. It uses an SD card for file storage and serves a responsive web UI to upload, download, and delete files in real time.  
*Supports SD cards up to 32 GB!*

## Features 🚀
- **WiFi Connection**: Connects to your network using a static IP.
- **SD Card Storage**: Uses an SD card (up to 32 GB, formatted as FAT32) to store files.
- **Responsive Web UI**: Manage files with a modern, User-friendly interface.
- **Real-Time Updates**: Uses WebSockets (port 81) to update the UI on file changes.
- **Folder Management**: Create and delete folders to organize your files.
- **Dynamic Storage Indicator**: Sums file sizes to show used, free, and total storage.

## Hardware Requirements ⚙️
- **ESP32 Doit DevKit V1**  
- **SD Card Module** (CS connected to pin 5 by default)  
- **SD Card**: Up to **32 GB** (formatted as FAT32)

## Software Requirements 🖥️
- [PlatformIO](https://platformio.org/) or [Arduino IDE](https://www.arduino.cc/en/software)  
- ESP32 board package (platform = `espressif32`)

## Environment Configuration 📋
Below is an example `platformio.ini` for PlatformIO:

```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
```

## Wiring Diagram 🔌
Connect the SD card module to the ESP32 as follows:
- **CS**: Pin 5 (or update in code if needed)
- **MOSI**, **MISO**, **SCK**: Connect to the corresponding SPI pins on the ESP32
- **Power & GND**: Connect to 5V and GND respectively

<p align="center">
  <img src="https://github.com/user-attachments/assets/8babf82b-86e3-4de6-b19c-37dd4dbd22a3" alt="Wiring Diagram" width="400">
</p>

## Screenshot 📸
Here’s a preview of the CloudSync UI:

![Screenshot 2025-03-28 183456](https://github.com/user-attachments/assets/da358558-a6e3-49c0-96d6-ffe7cbb43b86)

## How It Works 🤔
1. **WiFi & SD Initialization**:  
   The ESP32 connects to your WiFi network using a static IP and initializes the SD card.

2. **HTTP Server & WebSockets**:  
   - An HTTP server runs on port **80** to serve the UI and handle file operations.  
   - A WebSocket server on port **81** sends real-time notifications when files or folders are added or deleted.

3. **File and Folder Operations**:  
   - **Upload**: Files can be uploaded (and optionally placed in folders) and stored on the SD card.  
   - **Download**: Files can be downloaded directly from the UI.  
   - **Delete**: Files (or folders via the `/rmdir` endpoint) can be deleted.  
   - **List**: The `/list` endpoint returns a JSON array of files and folders (with optional filtering).  
   - **Storage Info**: The `/storage` endpoint recursively sums file sizes to display total, used, and free storage.

4. **Responsive UI**:  
   The web interface features a sidebar with a dynamic storage indicator and folder list (with options to create/delete folders) plus a main area for file management. The layout adjusts automatically for mobile devices.

## How to Flash 📡
1. Connect your ESP32 to your computer.  
2. Wire the SD card module according to the wiring diagram.  
3. Use PlatformIO or the Arduino IDE to compile and flash the code.  
4. Once flashed, open a browser and navigate to [http://192.168.1.100/](http://192.168.1.100/) to access CloudSync.

## License 📝
This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

---

Enjoy your very own Cloud-based Network Storage (NAS) built by **c1ph3r-1337**! 😎👍
```
