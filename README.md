# Smart Coaster

An ESP32-S3 powered drink mixing assistant that guides you through cocktail recipes step by step using a load cell, OLED display, and LED ring.

## Features

- **Step-by-step recipe guidance** — weigh each ingredient with real-time progress on the OLED and LED ring
- **Web app** — add, edit, and delete recipes from your phone via WiFi (AP mode)
- **Captive portal + mDNS** — connect to `smartcoaster.local` from any device
- **Battery powered** — 3.7V LiPo with voltage monitoring and charging detection
- **Deep sleep** — auto-sleep after inactivity with wake on button press
- **Configurable settings** — sound, screen brightness, LED brightness, WiFi password

## Bill of Materials

| Component | Specification | Qty |
|---|---|---|
| Seeed Studio XIAO ESP32-S3 | Microcontroller | 1 |
| SSD1306 OLED Display | 128x64, I2C, 0x3C address | 1 |
| HX711 Load Cell Amplifier | 24-bit ADC | 1 |
| Load Cell | Bar-type strain gauge | 1 |
| NeoPixel LED Ring | 24 LEDs, WS2812B | 1 |
| Passive Buzzer | 3.3V compatible | 1 |
| Tactile Buttons | Momentary push | 3 |
| 100K Resistors | 1/4W, 5% | 2 |
| LiPo Battery | 3.7V 700mAh | 1 |

## Wiring Diagram

### XIAO ESP32-S3 Pin Assignments

```
XIAO ESP32-S3 Pinout (top view)
 ___________
|           |
| D0 (GP1)  |--- Button Center (SELECT) --- GND
| D1 (GP2)  |--- Battery Voltage Divider (see below)
| D2 (GP3)  |--- NeoPixel LED Ring Data In
| D3 (GP4)  |--- HX711 DOUT (Data)
| D4 (GP5)  |--- SSD1306 SDA (I2C Data)
| D5 (GP6)  |--- SSD1306 SCL (I2C Clock)
| D6 (GP43) |--- Buzzer Signal
| D7 (GP44) |--- (unused)
| D8 (GP7)  |--- (unused)
| D9 (GP8)  |--- HX711 SCK (Clock)
| D10 (GP9) |--- Button Right --- GND
|           |
| 3V3       |--- OLED VCC, LED Ring VCC, HX711 VCC
| GND       |--- Common Ground
| BAT+      |--- LiPo Battery (+)
| BAT-      |--- LiPo Battery (-)
|  (GP10)   |--- Button Left --- GND
|___________|
```

### Battery Voltage Divider

Two 100K resistors form a voltage divider from BAT+ to GND, with the midpoint connected to D1 (GPIO2). This halves the battery voltage so the ADC can safely read it (max 3.3V input).

```
BAT+ ----[100K]----+----[100K]---- GND
                    |
                 D1 (GPIO2)
```

### SSD1306 OLED Display

| OLED Pin | XIAO Pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | D4 (GPIO5) |
| SCL | D5 (GPIO6) |

**Important:** The OLED must be powered from the **3V3 pin**, not VCC/5V. The 5V pin only has power when USB is connected — using it will cause the display to not work on battery.

### HX711 Load Cell Amplifier

| HX711 Pin | XIAO Pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DOUT | D3 (GPIO4) |
| SCK | D9 (GPIO8) |

### NeoPixel LED Ring (24 LEDs)

| Ring Pin | XIAO Pin |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DIN | D2 (GPIO3) |

### Buttons

All buttons connect between the GPIO pin and GND (using internal pull-up resistors).

| Button | XIAO Pin |
|---|---|
| Left | GPIO10 |
| Center (Select) | D0 (GPIO1) |
| Right | D10 (GPIO9) |

### Buzzer

| Buzzer Pin | XIAO Pin |
|---|---|
| Signal (+) | D6 (GPIO43) |
| GND (-) | GND |

## WiFi

On boot, the device creates a WiFi access point:

- **SSID:** SmartCoaster
- **Password:** cheers123 (changeable from web app)
- **URL:** http://smartcoaster.local or http://192.168.4.1

Connect with your phone to add/edit recipes and change settings.

## Libraries Required

Install these via the Arduino IDE Library Manager:

- **Adafruit SSD1306** (+ Adafruit GFX dependency)
- **HX711** by bogde
- **Adafruit NeoPixel**
- **ArduinoJson** by Benoit Blanchon

## Build & Upload

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support via Board Manager
3. Select board: **XIAO_ESP32S3**
4. Install required libraries
5. Open `smart-coaster/smart-coaster.ino`
6. Upload
