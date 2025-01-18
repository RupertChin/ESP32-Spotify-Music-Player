#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SPIFFS.h>
#include "esp_wifi.h"
#include "esp_wpa2.h"

#define LED_BUILTIN 2
#define INPUT_A 32
#define INPUT_B 33


// function defs
void connectToWiFi();
void connectToWiFiEnterprise();
void downloadAlbumArtSPIFFS();
bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
void updateDisplay();
void drawProgressBar();
bool refreshAccessToken();
void getCurrentlyPlayingTrack();
void IRAM_ATTR handleRot();

// global vars
const char *wifiSsid = "WIFI_SSID";
const char *wifiUsername = "WIFI_USERNAME"; // only needed if connecting to WPA2 Enterprise
const char *wifiPassword = "WIFI_PASSWORD";

// Spotify API credentials, change as necessary
const char *clientId = "CLIENT_ID";
const char *clientSecret = "CLIENT_SECRET";
const char *redirectUri = "http://localhost:8888/callback";
String accessToken = "ACCESS_TOKEN";
String refreshToken = "REFRESH_TOKEN";

// Current Song Information
String trackName = "Unavailable";
String artistName = "Unavailable";
String albumName = "Unavailable";
String albumArtUrl = "";
String prevAlbumArtUrl = "";
int currentSongPosition = 0;  // Current position in milliseconds
int songDuration = 0;        // Total duration in milliseconds

// other global vars
const int imageScaleSize = 2;
unsigned long prevMillis;

// TFT display
TFT_eSPI tft = TFT_eSPI();

// rotary encoder vars
int prevRotCounter = 0;
volatile int rotCounter = 0;
volatile bool clkState = LOW;
volatile bool dtState = LOW;

bool isConnected; // network connection status

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200); // why does it have to be 115200??? 921600 doesn't work even when same in ini file

  // rotary encoder input setup
  pinMode(INPUT_A, INPUT_PULLUP);
  pinMode(INPUT_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INPUT_A), handleRot, CHANGE);
  // attachInterrupt(digitalPinToInterrupt(INPUT_B), handleRot, CHANGE);

  /*
  * pick one of the following to connect to wifi: either wpa2 or wpa2-enterprise
  * also change the connectToWiFi function call in main loop
  */
  // connectToWiFi();
  connectToWiFiEnterprise();

  // initialize screen
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);

  if (SPIFFS.begin()) {
    Serial.println("SPIFFS mounted successfully");
  }
  else {
    Serial.println("SPIFFS mount failed");
  }
  
  // JPEG decoder
  TJpgDec.setJpgScale(imageScaleSize);
  TJpgDec.setCallback(tftOutput);

  prevMillis = millis();

  updateDisplay();
}

void loop() {
  // check for change in network connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection lost.");
    // connectToWiFi();
    connectToWiFiEnterprise();
  }

  // TODO just for testing, remove later
  if (rotCounter != prevRotCounter) {
    Serial.print("Rotary Counter: ");
    Serial.println(rotCounter);
    prevRotCounter = rotCounter;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - prevMillis >= 500) {
    getCurrentlyPlayingTrack();
    downloadAlbumArtSPIFFS();
    updateDisplay();
    prevMillis = currentMillis;
  }

  delay(10);
}

// helper functions
void connectToWiFi() {
  WiFi.begin(wifiSsid, wifiPassword);

  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }
  Serial.println("Connected!");
}

void connectToWiFiEnterprise() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wifiUsername, strlen(wifiUsername)) );
  ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wifiUsername, strlen(wifiUsername)) );
  ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wifiPassword, strlen(wifiPassword)) );
  esp_wifi_sta_wpa2_ent_enable();
  WiFi.begin(wifiSsid);

  // Wifi.begin(wifi_ssid, wifi_password); // WPA2 non-enterprise
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }
  Serial.println("Connected!");
}

void downloadAlbumArtSPIFFS() {
  if (albumArtUrl == prevAlbumArtUrl) {
    return;
  }

  Serial.println(albumArtUrl);
  Serial.println(prevAlbumArtUrl);

  HTTPClient http;
  http.begin(albumArtUrl);

  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    // delete previous album art
    if(SPIFFS.exists("/albumArt.jpg")) {
      SPIFFS.remove("/albumArt.jpg");
    }

    // write new album art
    File file = SPIFFS.open("/albumArt.jpg", "w");
    if(!file) {
      Serial.println("Failed to open file for writing");
      return;
    }

    // get http stream and write to file
    WiFiClient *stream = http.getStreamPtr();
    uint8_t buff[1024];
    size_t size = http.getSize();
    size_t remaining = size;

    while(http.connected() && (remaining > 0)) {
      size_t available = stream->available();
      size_t bytesToRead = min(available, sizeof(buff));
      
      if (bytesToRead > 0) {
          size_t bytesRead = stream->readBytes(buff, bytesToRead);
          if(bytesRead > 0) {
              file.write(buff, bytesRead);
              remaining -= bytesRead;
          }
      }
      yield(); 
    }

    file.close();
    prevAlbumArtUrl = albumArtUrl;
    Serial.println("Album art downloaded to SPIFFS");
  }
  else {
    Serial.print("Error downloading album art: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  }

  http.end();
}

bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void updateDisplay() {
  static String prevTrack = "";
  static String prevArtis = "";
  static String prevAlbum = "";

  if (prevTrack != trackName || prevArtis != artistName || prevAlbum != albumName) {
    tft.fillScreen(TFT_BLACK);

    if (SPIFFS.exists("/albumArt.jpg")) {
      // Initialize JPEG decoder
      TJpgDec.setJpgScale(imageScaleSize);    
      TJpgDec.setSwapBytes(true);
      TJpgDec.setCallback(tftOutput);

      // calculate position for image
      int imageX = (tft.width() - 150) / 2;
      int imageY = 20;

      // decode and display album art
      TJpgDec.drawFsJpg(imageX, imageY, "/albumArt.jpg");
    }

    // update text
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);

    tft.setCursor(10, 200);
    Serial.println(trackName);
    tft.print("Track: ");
    tft.println(trackName);

    tft.setCursor(10, 220);
    Serial.println(artistName);
    tft.print("Artist: ");
    tft.println(artistName);

    tft.setCursor(10, 240);
    Serial.println(albumName);
    tft.print("Album: ");
    tft.println(albumName);

    prevTrack = trackName;
    prevArtis = artistName;
    prevAlbum = albumName;
  }
}

bool refreshAccessToken() {
  HTTPClient http;
  http.begin("https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // payload formatted as form data
  String payload = "grant_type=refresh_token&refresh_token=" + refreshToken +
                   "&client_id=" + clientId +
                   "&client_secret=" + clientSecret;

  // send POST request for new access token
  int httpResponseCode = http.POST(payload);
  if (httpResponseCode == HTTP_CODE_OK) {
    String response = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      accessToken = doc["access_token"].as<String>();
      Serial.println("New Access Token: " + accessToken);
      return true;
    }
    else {
      Serial.print("JSON Deserialization Error: ");
      Serial.println(error.c_str());
    }
  }
  else {
    Serial.print("Error refreshing token: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  }

  http.end();
  return false;
}

void getCurrentlyPlayingTrack() {
  HTTPClient http;
  http.begin("https://api.spotify.com/v1/me/player/currently-playing"); // set market as CA?
  http.addHeader("Authorization", "Bearer " + accessToken);

  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    String response = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      // update current song info
      // Serial.println(doc["item"]["name"].as<String>());
      trackName = doc["item"]["name"].as<String>();
      artistName = doc["item"]["artists"][0]["name"].as<String>();
      albumName = doc["item"]["album"]["name"].as<String>();
      currentSongPosition = doc["progress_ms"].as<int>();
      songDuration = doc["item"]["duration_ms"].as<int>();

      JsonArray images = doc["item"]["album"]["images"];
      for (JsonVariant img : images) {
        if (img["height"] == 300) {
          albumArtUrl = img["url"].as<String>();
          break;
        }
      }
    }
    else {
      Serial.println("JSON Deserialization Error: ");
      Serial.println(error.c_str());
    }
  }
  else if (httpResponseCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("Access token expired, refreshing...");
    if (refreshAccessToken()) {
      getCurrentlyPlayingTrack();
    }
  }
  else {
    Serial.print("Error getting currently playing track: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  }
}

// interrupt handler for rotary encoder
void IRAM_ATTR handleRot() {
  static unsigned long lastInterruptTime = 0;

  // debounce
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime < 5) {
    return;
  }

  clkState = digitalRead(INPUT_A);
  dtState = digitalRead(INPUT_B);

  if (clkState != dtState) {
    rotCounter++;
  }
  else {
    rotCounter--;
  }

  lastInterruptTime = interruptTime;
}