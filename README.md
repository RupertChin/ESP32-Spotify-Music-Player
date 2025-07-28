# ESP32 Spotify Music Player

A real-time Spotify music player display built with ESP32, featuring album art, track information, and playback progress on a TFT display. Control volume using a rotary encoder and view your currently playing music with beautiful visuals.

<!-- ## Features

- 🎵 Real-time display of currently playing Spotify track
- 🖼️ Album art download and display
- ⏯️ Progress bar with time elapsed/remaining
- 🎛️ Volume control via rotary encoder
- 📱 Support for WPA2 and WPA2-Enterprise WiFi networks
- 💾 SPIFFS storage for album art caching -->

## Hardware Requirements

### Required Components
- **ESP32 Development Board** (ESP32-DevKit or similar)
- **TFT Display** - ILI9341 (240x320 pixels)
- **Rotary Encoder** - KY-040 or equivalent

### Pin Configuration

#### TFT Display (ILI9341)
| Display Pin | ESP32 Pin |
|-------------|-----------|
| VCC         | 3.3V      |
| GND         | GND       |
| CS          | GPIO 15   |
| RESET       | GPIO 2    |
| DC          | GPIO 4    |
| MOSI        | GPIO 23   |
| SCK         | GPIO 18   |
| MISO        | GPIO 19   |

#### Rotary Encoder
| Encoder Pin | ESP32 Pin |
|-------------|-----------|
| VCC         | 3.3V      |
| GND         | GND       |
| CLK (A)     | GPIO 32   |
| DT (B)      | GPIO 33   |

## Software Setup

### Prerequisites
- [PlatformIO](https://platformio.org/) installed in VS Code
- Python 3.x for token generation
- Spotify Premium account
- Spotify Developer App

### 1. Spotify API Setup

1. Go to https://developer.spotify.com/dashboard/
2. Create a new app
3. Set redirect URI to: `http://localhost:8888/callback`
4. Note down your `Client ID` and `Client Secret`

### 2. Get Spotify Tokens

1. Update the credentials in `src/get_token.py`:
```python
CLIENT_ID = 'your_client_id_here'
CLIENT_SECRET = 'your_client_secret_here'
```

2. Run the token generator:
```bash
cd src
python get_token.py
```

3. Follow the browser authorization and copy the tokens

### 3. Configure the ESP32 Code

1. Open `src/main.cpp` and update the following:

**WiFi Credentials:**
```cpp
const char *wifiSsid = "your_wifi_ssid";
const char *wifiPassword = "your_wifi_password";
```

**For WPA2-Enterprise (like eduroam):**
```cpp
const char *wifiUsername = "your_username@domain.com";
```

**Spotify API Credentials:**
```cpp
const char *clientId = "your_client_id";
const char *clientSecret = "your_client_secret";
String accessToken = "your_access_token";
String refreshToken = "your_refresh_token";
```

### 4. TFT Display Configuration

The display configuration is already set in `User_Setup.h`. If you need to modify pin assignments:

1. Navigate to `.pio/libdeps/esp32dev/TFT_eSPI/User_Setup.h`
2. Verify the pin definitions match your wiring:
```cpp
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    4
#define TFT_RST   2
```

## WiFi Connection Options

### Standard WPA2
Uncomment in `setup()`:
```cpp
connectToWiFi();
```

### WPA2-Enterprise (eduroam, corporate networks)
Uncomment in `setup()`:
```cpp
connectToWiFiEnterprise();
```

## Usage

1. **Power on the ESP32** - it will automatically connect to WiFi
2. **Start playing music on Spotify** - the display will show current track
3. **Use the rotary encoder** to control volume:
   - Rotate clockwise: Volume up
   - Rotate counter-clockwise: Volume down