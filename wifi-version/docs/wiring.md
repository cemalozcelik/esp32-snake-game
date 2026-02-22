# Wiring Diagram — ILI9341 + ESP32-S3

## Pin Table

| ILI9341 | ESP32-S3 | Description |
|---|---|---|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| CS | GPIO 14 | Chip Select |
| RESET | GPIO 21 | Display Reset |
| DC/RS | GPIO 47 | Data/Command |
| MOSI/SDA | GPIO 45 | SPI Data |
| SCK | GPIO 3 | SPI Clock |
| LED/BL | GPIO 48 | Backlight (PWM) |
| MISO | GPIO 46 | SPI Read |

## Power Note

The ESP32-S3 + ILI9341 + WiFi combined draw approximately ~400-600mA.
If your USB port provides insufficient power, use an external **5V 1A+** adapter.