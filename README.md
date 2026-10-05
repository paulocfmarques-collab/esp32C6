# ESP32-C6 IoT Gateway & Control System

<div align="center">

![ESP32-C6](https://img.shields.io/badge/Platform-ESP32--C6-blue)
![Language](https://img.shields.io/badge/Language-C%2B%2B-green)
![Status](https://img.shields.io/badge/Status-Active-brightgreen)

**A professional-grade IoT gateway system for the ESP32-C6 microcontroller with WiFi connectivity, real-time display, SD card logging, and comprehensive command processing.**

</div>

---

## Table of Contents

- [Overview](#overview)
- [Project Architecture](#project-architecture)
- [System Components](#system-components)
- [Hardware Schematic](#hardware-schematic)
- [Data Flow Diagram](#data-flow-diagram)
- [System State Machine](#system-state-machine)
- [Command Reference](#command-reference)
- [Installation & Setup](#installation--setup)
- [Configuration](#configuration)
- [Development](#development)
- [File Structure](#file-structure)
- [API Reference](#api-reference)

---

## Overview

The ESP32-C6 IoT Gateway & Control System is a sophisticated embedded application designed to provide:

- WiFi Connectivity: Station and Access Point modes
- Remote Command Processing: UDP-based command interface
- Real-Time Display: ST7789 display with rotation support
- LED Feedback: WS2812B RGB LED with breathing and blinking effects
- Data Logging: SD card-based persistent logging
- Network Synchronization: NTP for accurate time synchronization
- System Monitoring: Real-time CPU, RAM, and Flash metrics

### Key Features

- Automatic WiFi connection with fallback to configuration portal
- UDP command reception and processing
- HTML-based WiFi configuration interface
- Comprehensive system diagnostics
- SD card file management and logging
- RGB LED status indication
- Clock display with date and time
- Advanced command parsing with parameter validation

---

## Project Architecture

### High-Level Architecture Diagram

```mermaid
graph TB
    subgraph ESP32C6["ESP32-C6 Control System"]
        WiFi["WiFi Gateway<br/>- Station/AP<br/>- UDP Receiver<br/>- Portal Server"]
        Display["Display Manager<br/>- Clock Display<br/>- Status Lines<br/>- Rotation"]
        
        WiFi --> CMD["Command Processor<br/>- Parse commands<br/>- Execute actions<br/>- Log events"]
        Display --> CMD
        
        CMD --> LED["RGB LED Control"]
        CMD --> SD["SD Logic Logger"]
        CMD --> NTP["NTP Time Sync"]
        CMD --> Monitor["System Monitor"]
    end
```

### Layered Architecture

```mermaid
graph TB
    subgraph APP["APPLICATION LAYER"]
        Main["ESP32C6.ino<br/>- setup()<br/>- loop()<br/>- system orchestration"]
    end
    
    subgraph SERVICE["SERVICE LAYER"]
        CP["CommandProcessor<br/>- commands<br/>- logging<br/>- blinking"]
        GW["ESP32Gateway<br/>- WiFi<br/>- UDP<br/>- portal"]
        DU["DisplayUtil<br/>- TFT screen<br/>- clock display<br/>- text output"]
    end
    
    subgraph UTIL["UTILITY LAYER"]
        LED["RGBLed<br/>- LED effects"]
        NTP["NTPUtil<br/>- time sync"]
        SD["SDUtil<br/>- SD file ops"]
    end
    
    subgraph HAL["HARDWARE ABSTRACTION/DRIVERS"]
        HW["Arduino Core / WiFi / SPI / SD / Adafruit NeoPixel"]
    end
    
    subgraph HARDWARE["HARDWARE LAYER"]
        DEV["ESP32-C6 | ST7789 TFT | RGB LED | microSD | WiFi network"]
    end
    
    APP --> SERVICE
    SERVICE --> UTIL
    UTIL --> HAL
    HAL --> HARDWARE
```

---

## System Components

### 1. ESP32C6.ino - Main Application
The central sketch file that initializes all subsystems and runs the loop.

Responsibilities:
- Initialize serial output
- Initialize display, LED, gateway, SD, command processor
- Process received UDP commands
- Maintain system loop and clock display mode

### 2. ESP32Gateway - Network & Connectivity
This module handles:
- WiFi connection in Station mode
- WiFi Access Point mode when no credentials are stored
- UDP packet reception on port 4210
- Web server for WiFi configuration portal
- Memory persistence using Preferences

Flow:

```mermaid
flowchart TD
    START["START"]
    LOAD["Load saved SSID/password"]
    FOUND{"Config<br/>found?"}
    CONNECT["Connect WiFi"]
    PORTAL["Start config portal"]
    LED_G["LED green"]
    LED_Y["LED yellow"]
    WAIT["Wait for UDP command"]
    
    START --> LOAD
    LOAD --> FOUND
    FOUND -->|Yes| CONNECT
    FOUND -->|No| PORTAL
    CONNECT --> LED_G
    PORTAL --> LED_Y
    LED_G --> WAIT
    LED_Y --> WAIT
```

### 3. CommandProcessor - Command Execution Engine
This class processes received commands and dispatches actions.

Example supported commands:
- `LED_ON`
- `LED_OFF`
- `LED_PISCA:10:250`
- `LED_BLINK:500`
- `CPU`
- `RAM`
- `FLASH`
- `TIME`
- `NET_INFO`
- `LIST`
- `READ:/log.txt`
- `DEL:/file.txt`
- `RESET_WIFI`

### 4. DisplayUtil - TFT Display Controller
Responsibilities:
- ST7789 display initialization
- Output text to screen
- Rotation management
- Clock display mode
- Text history and redraw support

### 5. RGBLed - Visual Status Indicator
The RGB LED provides system feedback:
- Blue: trying to connect to WiFi
- Yellow: configuration portal active
- Green: connected successfully
- White: command output / active state
- Breathing / blinking: advanced status indicators

### 6. NTPUtil - Time Synchronization
Handles NTP time retrieval and formatted timestamps for logs and display.

### 7. SDUtil - Persistent Storage
Provides microSD card management such as:
- begin() and card health checks
- file read/write/append
- directory listing
- SD size and type detection
- log persistence in `/log.txt`

---

## Hardware Schematic

### Proposed System Wiring

```mermaid
graph TD
    ESP32["🔧 ESP32-C6<br/>WiFi + GPIO + SPI"]
    
    TFT["📺 ST7789 TFT Display"]
    SD["💾 microSD Card Slot"]
    LED["💡 WS2812B RGB LED"]
    
    ESP32 -->|SPI| TFT
    ESP32 -->|SPI| SD
    ESP32 -->|GPIO 8| LED
    
    SD_CS["CS = GPIO 4"]
    SD_MOSI["MOSI = GPIO 6"]
    SD_MISO["MISO = GPIO 5"]
    SD_SCLK["SCLK = GPIO 7"]
    
    SD --> SD_CS
    SD --> SD_MOSI
    SD --> SD_MISO
    SD --> SD_SCLK
    
    LED_GPIO["GPIO 8"]
    LED --> LED_GPIO
```

### Pin Mapping Summary

| Component | Pin | GPIO |
|-----------|-----|------|
| **SD Card** | CS | GPIO 4 |
| | MISO | GPIO 5 |
| | MOSI | GPIO 6 |
| | SCLK | GPIO 7 |
| **RGB LED** | DATA | GPIO 8 |
| **Display** | SPI | Board-specific |

---

## Data Flow Diagram

### Runtime Data Flow

```mermaid
sequenceDiagram
    participant Host as External Host<br/>Controller
    participant GW as ESP32Gateway
    participant CMD as CommandProcessor
    participant Display as DisplayUtil
    participant SD as SDUtil
    participant LED as RGBLed
    participant Response as UDP Response
    
    Host->>GW: UDP Packet (Command)
    GW->>CMD: receiveCommand()
    CMD->>CMD: parse command<br/>validate args<br/>dispatch call
    
    par Parallel Execution
        CMD->>Display: update UI
        CMD->>SD: append log
        CMD->>LED: status LED
    end
    
    CMD->>Response: Send Response
    Response-->>Host: UDP Response
```

### Initialization Data Flow

```mermaid
flowchart TD
    START["START"]
    SERIAL["Serial.begin()"]
    DISPLAY["display.begin()"]
    LED["rgbLed.begin()"]
    GATEWAY["gateway.begin()"]
    CMD["commandProcessor.begin()"]
    WIFI_CHECK{"WiFi<br/>connected?"}
    NTP["ntp.initNTP()"]
    READY["System Ready"]
    
    START --> SERIAL
    SERIAL --> DISPLAY
    DISPLAY --> LED
    LED --> GATEWAY
    GATEWAY --> CMD
    CMD --> WIFI_CHECK
    WIFI_CHECK -->|Yes| NTP
    WIFI_CHECK -->|No| READY
    NTP --> READY
```

---

## System State Machine

```mermaid
stateDiagram-v2
    [*] --> BOOT
    
    BOOT --> INIT: Initialize HW
    INIT --> WIFI_CHECK: WiFi Connect?
    
    WIFI_CHECK --> STA: Connected
    WIFI_CHECK --> AP: Not Found
    
    STA --> READY: NTP Sync
    AP --> CONFIG: Portal Active
    CONFIG --> READY: Credentials Saved
    
    READY --> CMD_RX: UDP Received
    CMD_RX --> EXECUTE: Parse Command
    EXECUTE --> READY: Log & Respond
    
    READY --> MONITOR: Monitor Mode
    MONITOR --> READY: Display Status
```

---

## Command Reference

### LED Commands

| Command | Description |
|---------|-------------|
| `LED_ON` | Turns the status LED on in white |
| `LED_OFF` | Turns the status LED off |
| `LED_PISCA:x:y` | Blink LED `x` times with interval `y` ms |
| `LED_BLINK:y` | Continuous blink with interval `y` ms |

Examples:
- `LED_PISCA:10:250`
- `LED_BLINK:500`

### System Commands

| Command | Description |
|---------|-------------|
| `CPU` | Returns CPU model, revision, and cores |
| `RAM` | Returns free heap and memory stats |
| `FLASH` | Returns flash memory size and stats |
| `INIT` | Returns reset reason |
| `UPTIME` | Returns device uptime |
| `MAC` | Returns MAC address |
| `NET_INFO` | Returns IP, gateway, mask, RSSI and SSID |
| `TIME` | Returns current date/time |

### SD Commands

| Command | Description |
|---------|-------------|
| `LIST` | List files in the SD root |
| `SD_TYPE` | Get SD card type |
| `SD_SIZE` | Get SD size in bytes |
| `SD_TEST` | Perform card test |
| `READ:/path/file.txt` | Read file from SD |
| `DEL:/path/file.txt` | Delete specified file |

### Configuration Commands

| Command | Description |
|---------|-------------|
| `RESET_WIFI` | Clears saved WiFi info and restarts |

---

## Installation & Setup

### Requirements

- ESP32-C6 development board
- Arduino IDE or VS Code + PlatformIO
- ST7789 display
- microSD card module or socket
- WS2812B RGB LED
- USB cable

### Arduino IDE Setup

1. Install the ESP32 board package
2. Select your board (`ESP32-C6` or compatible)
3. Install required libraries:
   - Arduino_GFX_Library
   - Adafruit_NeoPixel
   - SD
4. Open `ESP32C6.ino`
5. Compile and upload

### Required Libraries

- Adafruit NeoPixel
- Arduino_GFX_Library
- SD
- WiFi
- SPI

---

## Configuration

### WiFi Setup Flow

```mermaid
flowchart TD
    BOOT["Power On"]
    CHECK{"Saved WiFi<br/>config exists?"}
    AUTO["Connect automatically"]
    AP["Start AP mode<br/>ESP32_C6_CONFIG"]
    BROWSER["Open 192.168.4.1<br/>in browser"]
    SAVE["Save SSID/password"]
    REBOOT["Reboot device"]
    READY["Device Ready"]
    
    BOOT --> CHECK
    CHECK -->|Yes| AUTO
    CHECK -->|No| AP
    AUTO --> READY
    AP --> BROWSER
    BROWSER --> SAVE
    SAVE --> REBOOT
    REBOOT --> READY
```

### Portal HTML Form

The built-in portal is minimal and functional:
- SSID field
- Password field
- Save button

### NTP / Logging Configuration

- Logs are stored on SD under `/log.txt`
- Time is added to each log entry using NTP synchronization
- If NTP is unavailable, log entries are tagged as `Sem Hora Sinc.`

---

## Development

### File Structure

```
esp32C6/
├── ESP32C6.ino
├── CommandProcessor.h
├── CommandProcessor.cpp
├── ESP32Gateway.h
├── ESP32Gateway.cpp
├── DisplayUtil.h
├── DisplayUtil.cpp
├── RGBLed.h
├── RGBLed.cpp
├── NTPUtil.h
├── NTPUtil.cpp
├── SDUtil.h
├── SDUtil.cpp
├── README.md
└── LICENSE (optional)
```

### Class Relationship

```mermaid
graph TD
    Main["ESP32C6.ino"]
    
    Main --> Display["DisplayUtil"]
    Main --> LED["RGBLed"]
    Main --> Gateway["ESP32Gateway"]
    Main --> NTP["NTPUtil"]
    Main --> SD["SDUtil"]
    Main --> CMD["CommandProcessor"]
    
    CMD --> Display
    CMD --> Gateway
    CMD --> NTP
    CMD --> LED
    CMD --> SD
    
    style Main fill:#ff9999
    style Display fill:#99ccff
    style LED fill:#99ff99
    style Gateway fill:#ffcc99
    style NTP fill:#ff99cc
    style SD fill:#ccff99
    style CMD fill:#ffff99
```

---

## File Structure

### `ESP32C6.ino`
Main orchestrator. Initializes hardware and processes commands in the loop.

### `ESP32Gateway.*`
WiFi, configuration portal, UDP communication, and persistent storage.

### `CommandProcessor.*`
Handles command parsing, validation, logging, and system response generation.

### `DisplayUtil.*`
Controls the ST7789 display, text rendering, clock display, and rotation.

### `RGBLed.*`
Controls the RGB LED visual state feedback and effects.

### `NTPUtil.*`
Syncs the device clock using the NTP protocol.

### `SDUtil.*`
Implements microSD operations like reading, writing, deleting, and listing files.

---

## API Reference

### UDP Receive Interface

```cpp
bool ESP32Gateway::receiveCommand(String& comando)
```
This method reads a UDP packet from the configured port and returns the command string.

### Command Processing

```cpp
void CommandProcessor::executeCommand(String command)
```
This is the central command dispatcher for all supported actions.

### Response Mechanism

```cpp
void CommandProcessor::answerAll(String message, bool log = true)
```
Sends the message to:
- Serial monitor
- Display
- UDP response client
- SD log file when log is enabled

---

## Troubleshooting

### Problem: WiFi does not connect
Possible causes:
- stored SSID/password invalid
- weak signal
- device not in range

Fix:
- clear WiFi config with `RESET_WIFI`
- reconnect to the AP and configure again

### Problem: SD card not detected
Possible causes:
- card not properly seated
- wrong SPI pin wiring
- card is corrupted or not formatted

Fix:
- inspect wiring
- run `SD_TEST`
- format the card as FAT32

### Problem: No response to commands
Possible causes:
- wrong UDP port
- IP address mismatch
- firewall or network issue

Fix:
- check the device IP using `NET_INFO`
- send to port 4210
- confirm device is reachable on the LAN

### Problem: Display shows nothing
Possible causes:
- incorrect display power wiring
- SPI pins mismatched
- initialization issue

Fix:
- verify display wiring
- ensure proper board compatibility
- restart the device

---

## Notes

This project is intentionally designed for embedded experimentation and device automation. It demonstrates strong practices in modularization, connectivity, visual feedback, and logging.

---

<div align="center">

<strong>ESP32-C6 IoT Gateway Project</strong>

</div>
