# 🐍 Snake Game — WiFi WebSocket Edition

Snake game running on ESP32-S3, controlled wirelessly from any phone or browser via WebSocket.

```
ESP32-S3 creates its own WiFi hotspot → Connect your phone → Open 192.168.4.1 → Play!
```

---

## Hardware

| Component | Model |
|---|---|
| Microcontroller | ESP32-S3 |
| Display | ILI9341 (320×240, SPI) |

### Pin Connections

| ILI9341 Pin | ESP32-S3 Pin |
|---|---|
| SCLK | GPIO 3 |
| MOSI | GPIO 45 |
| MISO | GPIO 46 |
| DC | GPIO 47 |
| CS | GPIO 14 |
| RST | GPIO 21 |
| BL (Backlight) | GPIO 48 |

---

## Setup

### 1. Libraries

Install via Arduino IDE → Library Manager:

| Library | Fork / Version |
|---|---|
| LovyanGFX | Latest |
| AsyncTCP | **mathieucarbou** fork |
| ESPAsyncWebServer | **mathieucarbou** fork |

> ⚠️ Do **not** use the `lacamera` fork of ESPAsyncWebServer — it causes a FreeRTOS task conflict with AsyncTCP on ESP32-S3, resulting in a continuous watchdog reset loop.

### 2. Board Settings

Arduino IDE → Tools:

```
Board:  ESP32S3 Dev Module
Port:   (your ESP32's COM port)
```

### 3. Upload

Open `snake_game_wifi.ino` → Upload.

---

## How to Play

1. Power the ESP32 (USB or external 5V adapter)
2. On your phone, go to WiFi settings and connect to **SnakeGame**
   - Password: `12345678`
3. Open `192.168.4.1` in your browser
4. Tap **START** and play!

### Controls

| Button / Key | Action |
|---|---|
| ↑ / W | Move Up |
| ↓ / S | Move Down |
| ← / A | Move Left |
| → / D | Move Right |
| RESTART / R | Restart after game over |
| Enter | Start game |

---

## Architecture

```
[Phone / Browser]
      │
      ├── HTTP GET /       →  Serves the controller HTML page
      └── WebSocket /ws    →  Sends commands (U / D / L / R / S / RESTART)
                              Receives score & status as JSON
      │
[ESP32-S3]
   AsyncWebServer (port 80)   →  Serves HTML
   AsyncWebSocket  (/ws)      →  Handles commands, broadcasts score
      │
[ILI9341 Display]  →  Game field (320×240 px)
```

### Why AsyncWebServer + AsyncWebSocket only?

An earlier version used `ESPAsyncWebServer` together with a separate `WebSocketsServer` library. These two conflict at the FreeRTOS task level on ESP32-S3, causing continuous watchdog resets. `ESPAsyncWebServer` already includes `AsyncWebSocket` — no extra library needed, no manual polling in `loop()`, no resets.

---

## Power Note

ESP32-S3 + ILI9341 + active WiFi draws ~400–600 mA together. A USB port may not supply enough current, causing brownout resets. Use an external **5V 1A+** power adapter if the board keeps resetting.

---

## Project Structure

```
wifi-version/
├── arduino/
│   └── snake_game/
│       └── snake_game_wifi/
│           └── snake_game_wifi.ino
├── docs/
│   └── wiring.md
└── README.md
```
