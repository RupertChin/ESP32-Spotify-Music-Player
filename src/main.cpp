#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define LED_BUILTIN 2
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"

// function defs

bool refresh_access_token();
void get_currently_playing_track();

// global vars
const char *wifi_sside = WIFI_SSID;

// Spotify API credentials, change as necessary
// @todo change this stuff
const char *clientID = "a2d2559775754f199203f81403407117";
const char *clientSecret = "39bf4b692ca54b43a37117f8f091630d";
const char *redirectURI = "http://localhost:8888/callback"; // Change this to your redirect URI, maybe not needed can delete?
String accessToken = "BQAStWzMGy0icGfbgN57LVO0cEygDIloJ2nWh_qVk1p3JJ_fCNFwVpQM9zrWu9wfBM2ubJgWS3bJLYORgeCpdHqw-dYJr01Qc7WnvfzWO3_uLS5C_WCEi1JFjZYyhy-5RAr21caBxrlPw5CQg_vEBKCsD9kCh9T7v_6lfR03mJWxKr8N4nOB1wb821GNyxZHbGXHXWZGz426j1wsvvOM7_C6q1pJPNn7UbLr ";
String refreshToken = "AQAyWHE3tl944DkDwV-vnMiZ-8Lc30QGso-EQczWqg-rsVUBstTO59WzhkIMH7K2AtPY-19hqBHs1FyQQvwoBRBy9aPRXH8t6lBlrRz4kF3S3kNR3zEvfx7UFMvZfIFTD8o";

unsigned long previousMillis = 0; // Store the last time the track was checked
const long interval = 2500; // Interval to check the currently playing track (10 seconds)

bool isConnected; // network connection status

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200); // why does it have to be 115200??? 921600 doesn't work even when same in ini file
  
  isConnected = false;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting to WiFi...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }
  Serial.println("Connected!");
}

void loop() {
  // check for change in network connection status
  // @todo change to be blocking
  if (WiFi.status() == WL_CONNECTED && !isConnected) {
    Serial.println("connnected");
    digitalWrite(LED_BUILTIN, HIGH);
    isConnected = true;
  }
  else if (WiFi.status() == WL_DISCONNECTED && isConnected) {
    Serial.println("disconnected");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    isConnected = false;
    delay(500);
  }

  get_currently_playing_track();
  delay(1000);
}

// put function definitions here:
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
}
