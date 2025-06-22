# CTA Bus-O-Tron 🚌✨

**A real-time transit arrival indicator for Chicago CTA buses and trains using ESP8266 and dynamic LED displays.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP8266](https://img.shields.io/badge/Platform-ESP8266-blue.svg)](https://www.espressif.com/en/products/socs/esp8266)
[![Language: C++](https://img.shields.io/badge/Language-C%2B%2B-red.svg)](https://isocpp.org/)

## 🎯 Overview

The CTA Bus-O-Tron shows how far away the next bus is. I have a MQTT server that publishes arrival times pulled from the CTA API (multiple devices and Home Assistant use the info)

I wrote this to publish arrival ETAs to MQTT (https://github.com/leibert/ctaMQTT). This also has code to interface directly with the CTA APIs. I have a Mosquitto MQTT broker running as a home assistant add-on (https://www.home-assistant.io/integrations/mqtt/), but there's a number of MQTT brokers you can quickly spin-up

Code commenting and most of this readme AI generated, errors are likely present.

I care about a few routes near where I live in Lakeview Chicago. For rail you can get the stop IDs from CTA's documentation (https://www.transitchicago.com/developers/ttdocs/). For bus stop IDs, you can usually find the stop ID on the physical sign or you can find it on Google Maps, by clicking the Bus Stop, and the ID will be towards the top.

The box has mechanical relays that trigger in parallel with TO-220 FETs solely to make a fun mechanical sound (but to be silent at night). It is subscribed to an MQTT topic to turn the mechanical relays ON/OFF and a topic to turn off the Lights/Relays entirely.

- `devices/busotron/enable` - Master enable/disable ("ON"/"OFF")
- `CTApredictions/alert/active` - Alert status ("ON"/"OFF")


### Blinking/Clicking Speed 
- **Super Fast (180ms)**: Vehicle arriving very soon (< 1.5-3 minutes)
- **Fast (360ms)**: Vehicle approaching (< 5-6 minutes)  
- **Medium (600ms)**: Moderate wait time (< 10 minutes)
- **Slow (900ms)**: Longer wait time (< 15 minutes)
- **Solid On**: Very distant arrival (< 83 minutes)
- **Off**: No data or extremely distant arrival

### LED Colors
- **Amber Left**: Bus routes (Downtown Express, Route 77)
- **Red**: Brown Line trains 
- **Amber Right**: Route 151 buses
- **Tall Light**: System status/alert indicator

## 🔄 Display Cycle

The device cycles through routes every 10 seconds:

| Time | Route | Color | Direction |
|------|-------|-------|-----------|
| 0-9s | Downtown Express | Amber Left | ← |
| 10-19s | Route 77 West | Amber Left | → |
| 20-29s | Brown Line South | Red | ← |
| 30-39s | Brown Line North | Red | → |
| 40-49s | Route 151 South | Amber Right | ← |
| 50-59s | Route 151 North | Amber Right | → |

## 🛠️ Hardware Requirements

### ESP8266 Board
- NodeMCU, Wemos D1 Mini, or similar ESP8266 development board

### LEDs and Resistors
- 2x Directional arrow LEDs (left/right)
- 2x Amber indicator LEDs (left/right)
- 1x Red indicator LED (Brown Line)
- 1x Status/alert LED (tall light)
- Appropriate current-limiting resistors (typically 220-330Ω)

### Wiring Diagram

| Component | ESP8266 Pin | GPIO |
|-----------|-------------|------|
| Right Arrow LED | D4 | GPIO 2 |
| Left Arrow LED | D0 | GPIO 16 |
| Right Amber LED | D3 | GPIO 0 |
| Left Amber LED | D1 | GPIO 5 |
| Status Light | D8 | GPIO 15 |
| Red Line LED | D2 | GPIO 4 |

## 🚀 Quick Start

### 1. Clone the Repository
```bash
git clone https://github.com/yourusername/cta-busotron.git
cd cta-busotron
```

### 2. Create Secrets File
Copy the example secrets file and update with your credentials:
```bash
cp secrets_example.h secrets.h
```

Edit `secrets.h` with your network information:
```cpp
#define WIFI_SSID "your-wifi-network"
#define WIFI_PASSWORD "your-wifi-password"
#define MQTT_SERVER "your-mqtt-broker.com"
#define MQTT_USER "mqtt-username"
#define MQTT_PASSWORD "mqtt-password"
```

### 3. Install Dependencies
- [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/)
- ESP8266 board package
- PubSubClient library

### 4. Upload Code
1. Open `ctaBusOTron.ino` in Arduino IDE
2. Select your ESP8266 board and port
3. Upload the code

### 5. Monitor Serial Output
Open Serial Monitor (115200 baud) to see connection status and debug information.

## 📡 MQTT Topics

The Bus-O-Tron subscribes to these MQTT topics:

### Control Topics
- `devices/busotron/enable` - Master enable/disable ("ON"/"OFF")
- `CTApredictions/alert/active` - Alert status ("ON"/"OFF")

### Route Prediction Topics
- `CTApredictions/BUS/dtwnEXP` - Downtown Express
- `CTApredictions/BUS/1151/77` - Route 77 westbound
- `CTApredictions/RAIL/30231` - Brown Line northbound
- `CTApredictions/RAIL/30232` - Brown Line southbound
- `CTApredictions/BUS/1151/151` - Route 151 northbound
- `CTApredictions/BUS/1074/151` - Route 151 southbound

### Message Format
All prediction topics expect arrival time in seconds as payload:
```bash
# Example: Route 77 arriving in 4 minutes (240 seconds)
mosquitto_pub -h your-broker.com -t "CTApredictions/BUS/1151/77" -m "240"
```

## 🧪 Testing

### Manual Testing with Mosquitto
```bash
# Enable the system
mosquitto_pub -h your-broker.com -u username -P password \
  -t "devices/busotron/enable" -m "ON"

# Send test arrival time (3 minutes = 180 seconds)
mosquitto_pub -h your-broker.com -u username -P password \
  -t "CTApredictions/BUS/1151/77" -m "180"

# Activate alert
mosquitto_pub -h your-broker.com -u username -P password \
  -t "CTApredictions/alert/active" -m "ON"
```

### Serial Monitor Output
When working correctly, you should see:
```
>>> MQTT MESSAGE RECEIVED <<<
Topic: CTApredictions/BUS/1151/77
Length: 3
Payload: 180
>>> Updated Route 77 West ETA: 180 seconds
```

## 🔧 Configuration

### Adding New Routes
To add a new route, modify the `routes` array in the `BusOTron` class:

```cpp
Route routes[ROUTE_COUNT] = {
  // Add your new route here
  Route("New Route Name", "CTApredictions/BUS/new/topic", BUS_ROUTE, AMBER_LEFT, ARROW_LEFT, 6),
  // Existing routes...
};
```

Remember to:
1. Update `ROUTE_COUNT` constant
2. Add the new MQTT topic subscription
3. Adjust display timing if needed

### Customizing Blink Speeds
Modify the constants in `LEDController` class:
```cpp
static const int BLINK_SUPER_FAST = 180;  // Imminent arrival
static const int BLINK_FAST = 360;        // Approaching
static const int BLINK_MEDIUM = 600;      // Moderate wait
static const int BLINK_SLOW = 900;        // Longer wait
```

## 🏗️ Architecture

The code is organized into modular classes:

- **`BusOTron`**: Main orchestrator class
- **`LEDController`**: Manages all LED operations and blinking patterns
- **`NetworkManager`**: Handles WiFi and MQTT connections with auto-reconnect
- **`DisplayManager`**: Controls the cycling display and timing
- **`Route`**: Data structure for transit route definitions


## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Chicago Transit Authority** for providing real-time transit data
- **ESP8266 Community** for excellent documentation and libraries
- **PubSubClient Library** for robust MQTT implementation
- **Arduino Community** for the development platform
