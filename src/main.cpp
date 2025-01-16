#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "esp_wifi.h"
#include "esp_wpa2.h"

#define LED_BUILTIN 2
#define INPUT_CLK 32
#define INPUT_DT 33


// function defs
bool refresh_access_token();
void get_currently_playing_track();
void IRAM_ATTR handleRot();

// global vars
const char *wifi_ssid = "WIFI_SSID";
const char *wifi_username = "WIFI_USERNAME"; // only needed if connecting to WPA2 Enterprise
const char *wifi_password = "WIFI_PASSWORD";

// Spotify API credentials, change as necessary
const char *clientID = "7b624b0ac1c04f1497f52c0f332c240a";
const char *clientSecret = "214c9af7e36140ada003e766b4415854";
const char *redirectURI = "http://localhost:8888/callback";
String accessToken = "ACCESS_TOKEN";
String refreshToken = "REFRESH_TOKEN";

unsigned long previousMillis = 0; // Store the last time the track was checked
const long interval = 2500; // Interval to check the currently playing track (10 seconds)

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
  pinMode(INPUT_CLK, INPUT);
  pinMode(INPUT_DT, INPUT);
  attachInterrupt(digitalPinToInterrupt(INPUT_CLK), handleRot, CHANGE);
  
  isConnected = false;

  // WPA2 enterprise code starts here, comment out if not using
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
  ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wifi_username, strlen(wifi_username)) );
  ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wifi_username, strlen(wifi_username)) );
  ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wifi_password, strlen(wifi_password)) );
  esp_wifi_sta_wpa2_ent_enable();
  WiFi.begin(wifi_ssid);
  // WPA2 enterprise code ends here

  // Wifi.begin(wifi_ssid, wifi_password); // WPA2 non-enterprise
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }
  Serial.println("Connected!");
  isConnected = true;
}

void loop() {
  // check for change in network connection status
  // @todo change to be blocking
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("connnected");
    digitalWrite(LED_BUILTIN, HIGH);
    isConnected = true;
  }
  else if (WiFi.status() == WL_DISCONNECTED) {
    Serial.println("disconnected");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    isConnected = false;
    delay(500);
    return;
  }

  // // TODO just for testing, remove later
  // if (rotCounter != prevRotCounter) {
  // }
    Serial.print("Rotary Counter: ");
    Serial.println(rotCounter);
    prevRotCounter = rotCounter;

  get_currently_playing_track();
  delay(1000);
}

// helper functions
bool refresh_access_token() {
  HTTPClient http;
  http.begin("https://accounts.spotify.com/api/token");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // payload formatted as form data
  String payload = "grant_type=refresh_token&refresh_token=" + refreshToken +
                   "&client_id=" + clientID +
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

  return false;
}

void get_currently_playing_track() {
  HTTPClient http;
  http.begin("https://api.spotify.com/v1/me/player/currently-playing"); // set market as CA?
  http.addHeader("Authorization", "Bearer " + accessToken);

  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    String response = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (!error) {
      // TODO change to edit global variables
      Serial.println(doc["item"]["name"].as<String>());
    }
    else {
      Serial.println("JSON Deserialization Error: ");
      Serial.println(error.c_str());
    }
  }
  else if (httpResponseCode == HTTP_CODE_UNAUTHORIZED) {
    Serial.println("Access token expired, refreshing...");
    if (refresh_access_token()) {
      get_currently_playing_track();
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
  clkState = digitalRead(INPUT_CLK);
  dtState = digitalRead(INPUT_DT);

  if (clkState != dtState) {
    rotCounter++;
    Serial.println("Rotated right, volume go up"); // TODO change to edit global variables
  }
  else {
    rotCounter--;
    Serial.println("Rotated left, volume go down");
  }
}