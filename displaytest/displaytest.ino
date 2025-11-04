/*
Mein C++ tut weh
*/






















/*******************************************************************
    Hello World for the ESP32 Cheap Yellow Display.

    https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display

    If you find what I do useful and would like to support me,
    please consider becoming a sponsor on Github
    https://github.com/sponsors/witnessmenow/

    Written by Brian Lough
    YouTube: https://www.youtube.com/brianlough
    Twitter: https://twitter.com/witnessmenow
 *******************************************************************/

// Make sure to copy the UserSetup.h file into the library as
// per the Github Instructions. The pins are defined in there.

// Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
// Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
// Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
// Note the following larger fonts are primarily numeric only!
// Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
// Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
// Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// Transmitter option structure
struct TransmitterOption {
  String name;
  String actionUrl;
  String stateUrl;
  bool currentState;
};

// Screen state
enum ScreenState { MAIN_SCREEN, SETTINGS_SCREEN, WIFI_CONFIG_SCREEN, TRANSMITTER_SETUP_SCREEN, LOCK_SCREEN };

// Transmitter setup state
#define MAX_OPTIONS 10
extern TransmitterOption transmitterOptions[MAX_OPTIONS];
extern int numTransmitterOptions;
extern bool isLoadingOptions;
extern String lastError;
extern int scrollOffset;

// Initialize TFT display
TFT_eSPI tft = TFT_eSPI();

// Function to send a GET request to the specified URL
void phoneHome(const char* url) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.printf("Phone home successful. Response code: %d\n", httpResponseCode);
    } else {
      Serial.printf("Phone home failed. Error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    
    http.end();
  } else {
    Serial.println("WiFi not connected, skipping phone home");
  }
}

// Global variable to track theft protection status
bool isLocked = false;
bool unlockRequested = false;
bool isPadlockPressed = false;
unsigned long lastTheftCheck = 0;
unsigned long unlockStartTime = 0;
unsigned long lastDotUpdate = 0;
int dotCount = 0;
const unsigned long UNLOCK_TIMEOUT = 10000;      // 10 seconds for unlock countdown
const unsigned long THEFT_CHECK_INTERVAL = 300000; // 5 minutes
const unsigned long INITIAL_CHECK_DELAY = 5000;   // 5 seconds delay before first check
const unsigned long DOT_UPDATE_INTERVAL = 300;    // 300ms between dot animations
const int BOOT_BUTTON = 0;                       // GPIO0 is the boot button

// Padlock touch area
int padlockX, padlockY, padlockSize;

// Function to check if a point is within the padlock area
bool isTouchingPadlock(int x, int y) {
  return (x >= padlockX - padlockSize/2 && x <= padlockX + padlockSize/2 &&
          y >= padlockY - padlockSize/2 && y <= padlockY + padlockSize/2);
}

// Function to draw loading dots
void drawLoadingDots(int x, int y, int count) {
  int dotRadius = 4;
  int spacing = 10;
  for (int i = 0; i < 3; i++) {
    if (i < count) {
      tft.fillCircle(x + i * (dotRadius * 2 + spacing), y, dotRadius, TFT_BLACK);
    } else {
      tft.fillCircle(x + i * (dotRadius * 2 + spacing), y, dotRadius, TFT_LIGHTGREY);
    }
  }
}

// Function to draw a progress bar
void drawProgressBar(int x, int y, int width, int height, int progress) {
  // Draw background
  tft.drawRoundRect(x, y, width, height, 3, TFT_BLACK);
  // Draw progress
  int fillWidth = (width - 4) * progress / 100;
  tft.fillRoundRect(x + 2, y + 2, fillWidth, height - 4, 2, TFT_BLUE);
}

// Function to display startup animation with text
void displayStartupAnimation() {
  // Define the three words to display (customize these as needed)
  const char* words[] = {"Klasse 8A", "Jannis", "Elcin", "Adam", "Mr. T"};
  const int numWords = 5;
  const uint16_t textColor = TFT_WHITE;
  const uint16_t bgColor = TFT_BLACK;
  const int animationDuration = 1500; // ms per word
  const int pauseBetweenWords = 500;  // ms
  
  // Save current text settings
  uint8_t oldTextSize = tft.textsize;
  uint8_t oldTextDatum = tft.getTextDatum();
  uint16_t oldTextColor = tft.textcolor;
  uint16_t oldTextBgColor = tft.textbgcolor;
  
  // Set initial text properties
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM); // Middle center alignment
  tft.setTextColor(textColor, bgColor);
  
  // Calculate positions - center the text on screen
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;  // Center text vertically
  
  unsigned long startTime;
  
  for (int i = 0; i < numWords; i++) {
    startTime = millis();
    
    // Animation loop for each word
    while (millis() - startTime < animationDuration + pauseBetweenWords) {
      unsigned long elapsed = millis() - startTime;
      
      // Clear the screen for each frame
      tft.fillScreen(bgColor);
      
      if (elapsed < animationDuration) {
        // Calculate animation progress (0.0 to 1.0)
        float progress = (float)elapsed / animationDuration;
        
        // Ease-in-out function for smooth animation
        float easedProgress = (1 - cos(progress * PI)) / 2.0;
        
        // Calculate text size (grows from 1 to 4)
        int textSize = 1 + (easedProgress * 3);
        tft.setTextSize(textSize);
        
        // Calculate opacity (0 to 255)
        uint8_t alpha = 255 * easedProgress;
        tft.setTextColor(tft.alphaBlend(alpha, bgColor, textColor), bgColor);
        
        // Draw the word
        tft.drawString(words[i], centerX, centerY);
      } else {
        // Keep the final state until the pause is over
        tft.setTextSize(4);
        tft.setTextColor(textColor, bgColor);
        tft.drawString(words[i], centerX, centerY);
      }
      
      delay(200); // 20 FPS update rate
    }
  }
  
  // Fade out the last word
  startTime = millis();
  while (millis() - startTime < 500) {
    unsigned long elapsed = millis() - startTime;
    float progress = (float)elapsed / 500.0;
    uint8_t alpha = 255 * (1.0 - progress);
    
    tft.fillScreen(bgColor);
    
    // Fade out text
    tft.setTextSize(4);
    tft.setTextColor(tft.alphaBlend(alpha, bgColor, textColor), bgColor);
    tft.drawString(words[numWords-1], centerX, centerY);
    
    delay(50); // Keep the delay for smooth fade
  }
  
  // Clear the screen one last time
  tft.fillScreen(bgColor);
  
  // Restore original text settings
  tft.setTextSize(oldTextSize);
  tft.setTextDatum(oldTextDatum);
  tft.setTextColor(oldTextColor, oldTextBgColor);
}

// Function to check theft protection status and production state
void checkTheftProtection() {
  // Declare extern to access global variables
  extern bool isProductionMode;
  extern ScreenState currentScreen;
  
  Serial.println("Checking theft protection status...");
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping theft check and production state check");
    return;
  }

  // First check theft status
  HTTPClient http;
  http.begin("https://n8n.zvdxc.is-a.dev/webhook/theft");
  
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    bool wasLocked = isLocked;
    isLocked = (payload == "lock");
    Serial.printf("Theft check: %s\n", isLocked ? "LOCKED" : "unlocked");
    
    // Only check production state if not locked
    if (!isLocked) {
      bool newProductionState = checkProductionState();
      if (newProductionState != isProductionMode) {
        isProductionMode = newProductionState;
        Serial.print("Production mode changed to: ");
        Serial.println(isProductionMode ? "true" : "false");
        
        // Redraw the screen if we're on the main screen and not locked
        if (currentScreen == MAIN_SCREEN && !isLocked) {
          drawMainScreen();
        }
      }
    }
    
    if (isLocked) {
      // Clear screen and show lock with inverted colors
      tft.fillScreen(TFT_BLUE);
      tft.setTextColor(TFT_BLUE, TFT_BLACK);
      tft.setTextSize(2);
      tft.drawCentreString("DEVICE LOCKED", tft.width()/2, 30, 2);
      
      // Draw a lock symbol
      int centerX = tft.width() / 2;
      int centerY = tft.height() / 2;
      int lockWidth = 60;
      int lockHeight = 90;
      
      // Lock body
      tft.fillRoundRect(centerX - lockWidth/2, centerY - lockHeight/2, lockWidth, lockHeight, 5, TFT_BLACK);
      // Lock shackle
      tft.fillRect(centerX - lockWidth/4, centerY - lockHeight/2 - 15, lockWidth/2, 15, TFT_BLACK);
      tft.fillCircle(centerX, centerY - lockHeight/2 - 8, lockWidth/4, TFT_BLACK);
      // Keyhole
      tft.fillCircle(centerX, centerY, 10, TFT_WHITE);
      tft.fillRect(centerX - 2, centerY, 5, 15, TFT_WHITE);
      
      // Contact info
      tft.setTextSize(1);
      tft.drawCentreString("Contact support", tft.width()/2, tft.height() - 20, 2);
    }
  } else {
    Serial.printf("Theft check failed. Error: %s\n", http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
}

// Function to send unlock request
void sendUnlockRequest() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://n8n.zvdxc.is-a.dev/webhook/unlock");
    int httpCode = http.GET();
    http.end();
    
    if (httpCode > 0) {
      Serial.printf("Unlock request sent. Status code: %d\n", httpCode);
    } else {
      Serial.printf("Unlock request failed. Error: %s\n", http.errorToString(httpCode).c_str());
    }
  } else {
    Serial.println("WiFi not connected, cannot send unlock request");
  }
}

bool toggleTransmitterOption(int index) {
  if (index < 0 || index >= numTransmitterOptions) return false;
  
  // Show loading state
  bool oldState = transmitterOptions[index].currentState;
  transmitterOptions[index].currentState = !oldState;
  drawTransmitterSetupScreen();
  
  // Make the API call
  HTTPClient http;
  String url = transmitterOptions[index].actionUrl + (oldState ? "?state=off" : "?state=on");
  http.begin(url);
  http.setTimeout(3000);
  
  bool success = false;
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (!error && doc.containsKey("success") && doc["success"]) {
      success = true;
      // Update state from server
      if (doc.containsKey("state")) {
        transmitterOptions[index].currentState = doc["state"];
      }
    }
  }
  
  http.end();
  
  if (!success) {
    // Revert on failure
    transmitterOptions[index].currentState = oldState;
  }
  
  // Redraw with updated state
  drawTransmitterSetupScreen();
  return success;
}

// Function to draw the lock screen
void drawLockScreen(bool showUnlockMessage) {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(2);
  
  // Calculate positions - moved down by 20 pixels to prevent clipping
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 3 + 30;  // Moved down further to prevent clipping
  int lockWidth = 60;
  int lockHeight = 80;
  
  // Store padlock position for touch detection
  padlockX = centerX;
  padlockY = centerY;
  padlockSize = lockWidth > lockHeight ? lockWidth : lockHeight;
  
  // Draw lock symbol with pressed state
  uint16_t lockColor = isPadlockPressed ? TFT_DARKGREY : TFT_BLACK;
  
  // Lock body
  tft.fillRoundRect(centerX - lockWidth/2, centerY - lockHeight/2, lockWidth, lockHeight, 5, lockColor);
  // Lock shackle
  tft.fillRect(centerX - lockWidth/4, centerY - lockHeight/2 - 15, lockWidth/2, 15, lockColor);
  tft.fillCircle(centerX, centerY - lockHeight/2 - 8, lockWidth/4, lockColor);
  // Keyhole (white on black)
  tft.fillCircle(centerX, centerY, 10, TFT_WHITE);
  tft.fillRect(centerX - 2, centerY, 5, 15, TFT_WHITE);
  
  // Status text (top of screen)
  tft.drawCentreString("DEVICE LOCKED", tft.width()/2, 10, 2);
  
  // Text Y position below the padlock - adjusted for new layout
  int textY = centerY + lockHeight/2 + 15;
  
  if (unlockRequested) {
    // Show loading dots
    drawLoadingDots(centerX - 25, textY + 10, dotCount);
    
    // Show progress bar at the bottom
    unsigned long elapsed = millis() - unlockStartTime;
    if (elapsed < UNLOCK_TIMEOUT) {
      int progress = 100 - (elapsed * 100 / UNLOCK_TIMEOUT);
      drawProgressBar(20, tft.height() - 30, tft.width() - 40, 15, progress);
    }
  } else if (showUnlockMessage) {
    // Show touch instruction
    tft.drawCentreString("Touch padlock to", tft.width()/2, textY, 2);
    tft.drawCentreString("request unlock", tft.width()/2, textY + 25, 2);
  }
  
  // No contact info shown - removed as per request
}
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>
#include "wifi_config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

// Touch screen pins (CYD specific)
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Initialize SPI for touch controller
SPIClass mySpi(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// Create WiFi manager instance
WiFiManager wifiManager;

// Touch calibration values - adjust these based on your display
#define TOUCH_MIN_X 150
#define TOUCH_MAX_X 3600
#define TOUCH_MIN_Y 200
#define TOUCH_MAX_Y 3700


// Global variables for transmitter setup
TransmitterOption transmitterOptions[MAX_OPTIONS];
int numTransmitterOptions = 0;
bool isLoadingOptions = false;
bool isProductionMode = false;  // Track if device is in production mode
String lastError = "";
int scrollOffset = 0;
ScreenState currentScreen = MAIN_SCREEN;

// Settings button position and size (moved to top-left corner for now)
#define SETTINGS_BTN_X 0
#define SETTINGS_BTN_Y 0
#define SETTINGS_BTN_W 40
#define SETTINGS_BTN_H 40

// WiFi check interval (ms)
const unsigned long WIFI_CHECK_INTERVAL = 10000;
unsigned long lastWifiCheck = 0;

// Decibel update interval (ms)
const unsigned long DECIBEL_UPDATE_INTERVAL = 1000;
unsigned long lastDecibelUpdate = 0;
float currentDecibel = 0.0;

// Check if a point is within a rectangle
bool isInRect(int16_t x, int16_t y, int16_t rectX, int16_t rectY, int16_t rectW, int16_t rectH) {
  return (x >= rectX) && (x < (rectX + rectW)) && (y >= rectY) && (y < (rectY + rectH));
}
void drawSettingsIcon(int16_t x, int16_t y, uint16_t color, uint16_t bgColor) {
  // Draw a simple gear icon
  tft.fillCircle(x + 8, y + 8, 8, color);
  tft.fillCircle(x + 8, y + 8, 4, bgColor);
  
  // Draw gear teeth
  for (int i = 0; i < 8; i++) {
    float angle = i * PI / 4;
    int x1 = x + 8 + (int)(5 * cos(angle));
    int y1 = y + 8 + (int)(5 * sin(angle));
    int x2 = x + 8 + (int)(8 * cos(angle));
    int y2 = y + 8 + (int)(8 * sin(angle));
    tft.drawLine(x1, y1, x2, y2, color);
  }
}
// Function to check if WiFi is connected
bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// Function to check production state from server
bool checkProductionState() {
  if (!isWiFiConnected()) {
    Serial.println("Not connected to WiFi, can't check production state");
    return false;
  }

  HTTPClient http;
  String url = "https://n8n.zvdxc.is-a.dev/webhook/getprodstate";
  
  http.begin(url);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    Serial.println("Production state: " + payload);
    http.end();
    return payload == "true";
  } else {
    Serial.printf("Failed to get production state. HTTP error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}

// Function to check if device is in AP mode
bool isAPMode() {
  return WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
}

// Function to reboot the device
void rebootDevice() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawCentreString("Rebooting...", tft.width()/2, tft.height()/2, 2);
  delay(1000);
  ESP.restart();
}

// Function to draw the settings screen
void drawSettingsScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  
  // Title
  tft.drawCentreString("Settings", tft.width()/2, 10, 4);
  
  // WiFi Setup Button
  tft.drawRect(40, 40, 240, 35, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.drawCentreString("WiFi Setup", tft.width()/2, 50, 2);
  
  // Setup Transmitter Button
  tft.drawRect(40, 90, 240, 35, TFT_GREEN);
  tft.setTextColor(TFT_DARKGREEN, TFT_WHITE);
  tft.drawCentreString("Setup Transmitter", tft.width()/2, 100, 2);
  
  // Reboot button (only in AP mode)
  if (isAPMode()) {
    tft.drawRect(40, 140, 240, 35, TFT_ORANGE);
    tft.setTextColor(TFT_ORANGE, TFT_WHITE);
    tft.drawCentreString("Reboot Device", tft.width()/2, 150, 2);
  }
  
  // Exit option
  int exitButtonY = isAPMode() ? 190 : 140; // Adjust position based on reboot button visibility
  tft.drawRect(40, exitButtonY, 240, 35, TFT_RED);
  tft.setTextColor(TFT_RED, TFT_WHITE);
  tft.drawCentreString("EXIT", tft.width()/2, exitButtonY + 10, 2);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

bool fetchTransmitterOptions() {
  Serial.println("Fetching transmitter options...");
  
  if (WiFi.status() != WL_CONNECTED) {
    lastError = "WiFi not connected";
    Serial.println("Error: " + lastError);
    return false;
  }

  HTTPClient http;
  String url = "https://n8n.zvdxc.is-a.dev/webhook/transmitter-options";
  Serial.println("Making request to: " + url);
  
  http.begin(url);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  Serial.print("HTTP status code: ");
  Serial.println(httpCode);
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Response: " + payload);
    
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    http.end();
    
    if (error) {
      lastError = "JSON parse failed: " + String(error.c_str());
      Serial.println("Error: " + lastError);
      return false;
    }
    
    // Clear existing options
    numTransmitterOptions = 0;
    
    // Parse options
    if (!doc.is<JsonArray>()) {
      lastError = "Expected JSON array";
      Serial.println("Error: " + lastError);
      return false;
    }
    
    JsonArray options = doc.as<JsonArray>();
    Serial.print("Found ");
    Serial.print(options.size());
    Serial.println(" options in response");
    
    for (JsonObject option : options) {
      if (numTransmitterOptions >= MAX_OPTIONS) break;
      
      if (!option.containsKey("name") || !option.containsKey("actionUrl") || !option.containsKey("stateUrl")) {
        Serial.println("Skipping invalid option (missing required fields)");
        continue;
      }
      
      transmitterOptions[numTransmitterOptions].name = option["name"].as<String>();
      transmitterOptions[numTransmitterOptions].actionUrl = option["actionUrl"].as<String>();
      transmitterOptions[numTransmitterOptions].stateUrl = option["stateUrl"].as<String>();
      transmitterOptions[numTransmitterOptions].currentState = false; // Will be updated in updateTransmitterState
      
      Serial.print("Added option: ");
      Serial.println(transmitterOptions[numTransmitterOptions].name);
      
      numTransmitterOptions++;
    }
    
    Serial.print("Successfully parsed ");
    Serial.print(numTransmitterOptions);
    Serial.println(" options");
    
    // Update states
    for (int i = 0; i < numTransmitterOptions; i++) {
      updateTransmitterState(i);
    }
    
    return true;
  } else {
    lastError = "HTTP error: " + String(httpCode);
    Serial.println("Error: " + lastError);
    http.end();
    return false;
  }
}

bool updateTransmitterState(int index) {
  if (index < 0 || index >= numTransmitterOptions) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  http.begin(transmitterOptions[index].stateUrl);
  http.setTimeout(3000);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, http.getStream());
    http.end();
    
    if (!error) {
      transmitterOptions[index].currentState = doc["state"];
      return true;
    }
  }
  
  http.end();
  return false;
}

void drawTransmitterSetupScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  
  // Title
  tft.drawCentreString("Transmitter Setup", tft.width()/2, 10, 4);
  
  if (isLoadingOptions) {
    tft.drawCentreString("Loading...", tft.width()/2, 60, 2);
    return;
  }
  
  // Content area starts right below the title
  int contentStartY = 50;
  
  if (lastError.length() > 0) {
    tft.setTextColor(TFT_RED, TFT_WHITE);
    tft.drawCentreString("Error: " + lastError, tft.width()/2, 60, 2);
    tft.drawCentreString("Tap to retry", tft.width()/2, 90, 2);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
  } else if (numTransmitterOptions == 0) {
    tft.drawCentreString("No options available", tft.width()/2, 60, 2);
  } else {
    // Draw options list (up to 4 visible at a time)
    int y = 50;
    int visibleOptions = min(4, numTransmitterOptions - scrollOffset);
    
    for (int i = 0; i < visibleOptions; i++) {
      int idx = i + scrollOffset;
      if (idx >= numTransmitterOptions) break;
      
      // Option background
      tft.drawRect(20, y - 20, 280, 40, TFT_LIGHTGREY);
      
      // Option name
      tft.setCursor(30, y);
      tft.print(transmitterOptions[idx].name);
      
      // State indicator with text (true = purple/On, false = blue/Off)
      uint16_t stateColor = transmitterOptions[idx].currentState ? TFT_PURPLE : TFT_BLUE;
      String stateText = transmitterOptions[idx].currentState ? "On" : "Off";
      
      // Draw the state circle
      tft.fillCircle(270, y + 2, 8, stateColor);
      
      // Draw the state text next to the circle
      tft.setCursor(285, y);
      tft.print(stateText);
      
      y += 45;
    }
    
    // Scroll indicators
    if (scrollOffset > 0) {
      tft.fillTriangle(20, 40, 30, 30, 40, 40, TFT_BLACK);
    }
    if (scrollOffset + 4 < numTransmitterOptions) {
      tft.fillTriangle(20, 220, 30, 230, 40, 220, TFT_BLACK);
    }
  }
  
  // Calculate positions for action buttons
  int actionButtonsY = 50 + (min(4, numTransmitterOptions) * 45) + 10; // 10px space after options
  
  // Refresh button (above back button)
  tft.drawRect(20, actionButtonsY, 280, 40, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.drawCentreString("↻ Refresh Options", tft.width()/2, actionButtonsY + 12, 2);
  
  // Back button (at the bottom)
  int backButtonY = actionButtonsY + 50; // 10px spacing between buttons
  tft.drawRect(20, backButtonY, 280, 40, TFT_RED);
  tft.setTextColor(TFT_RED, TFT_WHITE);
  tft.drawCentreString("← Back to Settings", tft.width()/2, backButtonY + 12, 2);
  
  // Reset text color to default
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

void drawWiFiConfigScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  
  // Title
  tft.drawCentreString("WiFi Setup", tft.width()/2, 20, 4);
  
  tft.drawCentreString("Connect to WiFi:", tft.width()/2, 60, 2);
  tft.drawCentreString("SSID: CYD_Config", tft.width()/2, 90, 2);
  tft.drawCentreString("Open: http://192.168.4.1", tft.width()/2, 120, 2);
  
  // Back button
  tft.drawRect(40, 160, 240, 40, TFT_BLUE);
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.drawCentreString("BACK", tft.width()/2, 170, 2);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

bool fetchDecibelValue() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi");
    return false;
  }

  HTTPClient http;
  bool success = false;
  
  // Initialize HTTP client
  http.begin("https://n8n.zvdxc.is-a.dev/webhook/getdecibel");
  http.setTimeout(5000); // 5 second timeout
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == HTTP_CODE_OK) {
    String payload = http.getString();
    payload.trim();
    
    // Try to convert response to float
    float newValue = payload.toFloat();
    if (newValue > 0) {
      currentDecibel = newValue;
      success = true;
      Serial.print("Updated decibel value: ");
      Serial.println(currentDecibel, 1);
    } else {
      Serial.println("Invalid decibel value received");
    }
  } else {
    Serial.print("HTTP request failed, error: ");
    Serial.println(http.errorToString(httpResponseCode).c_str());
  }
  
  http.end();
  return success;
}

// Function to draw anti-aliased text with a larger font
void drawAAText(String text, int x, int y, int size, uint16_t color, uint16_t bgcolor) {
  // Draw the text multiple times with small offsets to create anti-aliasing
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx != 0 || dy != 0) {  // Skip the center position for now
        tft.setTextColor(bgcolor, bgcolor);  // Background color for the "shadow"
        tft.drawString(text, x + dx, y + dy, size);
      }
    }
  }
  // Draw the main text last
  tft.setTextColor(color, bgcolor);
  tft.drawString(text, x, y, size);
}

void drawMainScreen() {
  tft.fillScreen(TFT_WHITE);
  
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  int fontNum = 8;  // Using Font 8 for the large number
  String numberText = String(currentDecibel, 1); // Show 1 decimal place
  
  // Calculate text dimensions
  int textWidth = tft.textWidth(numberText, fontNum);
  int textHeight = 75; // Font 8 is 75 pixels high
  
  // Calculate x and y coordinates to center the main number with the "db" text
  int dbFontNum = 4; // Smaller font for "db"
  String dbText = " db";
  int dbTextWidth = tft.textWidth(dbText, dbFontNum);
  
  // Calculate the total width of number + "db"
  int totalWidth = textWidth + dbTextWidth;
  
  // Calculate starting x position to center the whole text
  int x = (tft.width() - totalWidth) / 2;
  int y = (tft.height() - textHeight) / 2;
  
  // Calculate Y offset for "db" text (position it relative to the number)
  int dbTextYOffset = 20;  // Base offset in pixels
  float scale = 1.15f;     // Scale factor for larger text
  dbTextYOffset = dbTextYOffset * scale;  // Scale the offset
  
  // Draw the number with anti-aliasing and increased size
  drawAAText(numberText, x, y, fontNum, TFT_BLACK, TFT_WHITE);
  
  // Reset text size for other elements
  tft.setTextSize(1);
  
  // Draw the "db" text normally (smaller text doesn't need anti-aliasing as much)
  tft.drawString(dbText, x + textWidth, y + textHeight - dbTextYOffset, dbFontNum);
  
  // Only show settings and IP if not in production mode
  if (!isProductionMode) {
    // Draw settings icon in top-left corner
    drawSettingsIcon(SETTINGS_BTN_X, SETTINGS_BTN_Y, TFT_BLACK, TFT_WHITE);
    
    // Show WiFi status
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    String wifiStatus = wifiManager.isAPMode() ? "AP Mode" : 
                       (wifiManager.isConnected() ? WiFi.localIP().toString() : "No WiFi");
    tft.drawString(wifiStatus, 10, tft.height() - 20, 2);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
  }
}
void loop() {
  // Check theft protection status periodically
  static bool firstCheck = true;
  if (millis() - lastTheftCheck >= THEFT_CHECK_INTERVAL || firstCheck) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n=== Performing Theft Check ===");
      checkTheftProtection();
      lastTheftCheck = millis();
      firstCheck = false;
    } else if (firstCheck) {
      // If it's the first check and WiFi isn't ready, reschedule it
      lastTheftCheck = millis() - THEFT_CHECK_INTERVAL + 2000; // Try again in 2 seconds
      Serial.println("WiFi not ready, delaying first theft check...");
    }
  }

  // Handle unlock countdown and reboot
  if (unlockRequested && (millis() - unlockStartTime >= UNLOCK_TIMEOUT)) {
    ESP.restart();
  }
  
  // If device is locked, only show lock screen and handle unlock button
  if (isLocked) {
    // Handle touch input for padlock
    static bool wasTouching = false;
    bool isTouching = ts.tirqTouched() && ts.touched();
    
    if (isTouching) {
      TS_Point p = ts.getPoint();
      int16_t touchX = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, tft.width(), 0);
      int16_t touchY = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, tft.height());
      touchX = tft.width() - touchX;
      
      if (isTouchingPadlock(touchX, touchY) && !unlockRequested) {
        if (!wasTouching) {  // Only trigger on new touch
          isPadlockPressed = true;
          drawLockScreen(true);  // Update with pressed state
        }
      }
      wasTouching = true;
    } else {
      // Handle touch release
      if (wasTouching && isPadlockPressed && !unlockRequested) {
        // Only trigger unlock if the touch was released over the padlock
        isPadlockPressed = false;
        unlockRequested = true;
        unlockStartTime = millis();
        lastDotUpdate = millis();
        sendUnlockRequest();
        Serial.println("Unlock requested, will reboot in 10 seconds");
        drawLockScreen(false);
      }
      wasTouching = false;
      isPadlockPressed = false;
    }
    
    // Update lock screen if needed
    if (currentScreen != LOCK_SCREEN) {
      currentScreen = LOCK_SCREEN;
      drawLockScreen(!unlockRequested);
    }
    
    // Handle countdown and animations
    unsigned long currentMillis = millis();
    
    if (unlockRequested) {
      // Update loading dots animation
      if (currentMillis - lastDotUpdate >= DOT_UPDATE_INTERVAL) {
        dotCount = (dotCount % 3) + 1;  // Cycle through 1-3 dots
        lastDotUpdate = currentMillis;
        
        // Only redraw if we're not at the start of a new second
        unsigned long elapsed = currentMillis - unlockStartTime;
        unsigned long currentSecond = (UNLOCK_TIMEOUT - elapsed) / 1000;
        static unsigned long lastSecond = 0;
        
        if (currentSecond != lastSecond) {
          lastSecond = currentSecond;
          drawLockScreen(false);  // Full redraw on second change
        } else {
          // Just update the dots area - adjusted Y position
          int centerX = tft.width() / 2;
          int textY = (tft.height() / 3 + 30) + 80/2 + 15; // Adjusted Y position
          drawLoadingDots(centerX - 25, textY + 10, dotCount);
        }
      }
      
      // Check if countdown is complete
      if (currentMillis - unlockStartTime >= UNLOCK_TIMEOUT) {
        ESP.restart();
      }
    }
    
    return;  // Skip the rest of the loop when locked
  }

  // Update decibel value periodically
  if (millis() - lastDecibelUpdate >= DECIBEL_UPDATE_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      if (fetchDecibelValue() && currentScreen == MAIN_SCREEN) {
        drawMainScreen();
      }
    }
    lastDecibelUpdate = millis();
  }
  
  // Handle touch input
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    if (p.z > 10) {
      int16_t touchX = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, tft.width(), 0);
      int16_t touchY = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, tft.height());
      touchX = tft.width() - touchX;
      touchX = constrain(touchX, 0, tft.width());
      touchY = constrain(touchY, 0, tft.height());

      Serial.printf("Touch at X: %d, Y: %d\n", touchX, touchY);

      if (currentScreen == MAIN_SCREEN) {
        if (isInRect(touchX, touchY, SETTINGS_BTN_X, SETTINGS_BTN_Y, SETTINGS_BTN_W, SETTINGS_BTN_H)) {
          // Only allow settings if not locked
          if (!isLocked) {
            currentScreen = SETTINGS_SCREEN;
            // Phone home when settings is opened
            phoneHome("https://n8n.zvdxc.is-a.dev/webhook/settingsopened");
            drawSettingsScreen();
          }
        }
      } else if (currentScreen == SETTINGS_SCREEN) {
        // WiFi Setup button
        if (isInRect(touchX, touchY, 40, 40, 240, 35)) {
          currentScreen = WIFI_CONFIG_SCREEN;
          wifiManager.startAP();
          drawWiFiConfigScreen();
        } 
        // Setup Transmitter button
        else if (isInRect(touchX, touchY, 40, 90, 240, 35)) {
          currentScreen = TRANSMITTER_SETUP_SCREEN;
          // Clear any previous error and fetch options
          lastError = "";
          scrollOffset = 0;
          isLoadingOptions = true;
          drawTransmitterSetupScreen();
          fetchTransmitterOptions();
          isLoadingOptions = false;
          drawTransmitterSetupScreen();
        }
        // Exit button
        else if (isInRect(touchX, touchY, 40, 180, 240, 35)) {
          currentScreen = MAIN_SCREEN;
          drawMainScreen();
        }
      } else if (currentScreen == WIFI_CONFIG_SCREEN) {
        // Back button - trigger reboot when exiting WiFi config
        if (isInRect(touchX, touchY, 40, 180, 240, 35)) {
          // Show rebooting message
          tft.fillScreen(TFT_WHITE);
          tft.setTextColor(TFT_BLACK, TFT_WHITE);
          tft.drawCentreString("Rebooting to apply", tft.width()/2, tft.height()/2 - 20, 2);
          tft.drawCentreString("network settings...", tft.width()/2, tft.height()/2 + 10, 2);
          delay(1500);
          ESP.restart();
        }
      } else if (currentScreen == TRANSMITTER_SETUP_SCREEN) {
        // Calculate button positions (matches the drawing code)
        int actionButtonsY = 50 + (min(4, numTransmitterOptions) * 45) + 10;
        int refreshButtonY = actionButtonsY;
        int backButtonY = actionButtonsY + 50;
        
        // Check if refresh button was pressed
        if (isInRect(touchX, touchY, 20, refreshButtonY, 280, 40)) {
          // Refresh button
          isLoadingOptions = true;
          drawTransmitterSetupScreen();
          fetchTransmitterOptions();
          isLoadingOptions = false;
          drawTransmitterSetupScreen();
        }
        // Check if back button was pressed
        else if (isInRect(touchX, touchY, 20, backButtonY, 280, 40)) {
          currentScreen = SETTINGS_SCREEN;
          drawSettingsScreen();
        }
        // Check if an option was tapped (in the list area, above action buttons)
        else if (touchY >= 50 && touchY < actionButtonsY) {
          int optionHeight = 45;
          int visibleOptions = min(4, numTransmitterOptions - scrollOffset);
          int clickedOption = (touchY - 50) / optionHeight;
          
          if (clickedOption >= 0 && clickedOption < visibleOptions) {
            int optionIndex = scrollOffset + clickedOption;
            if (optionIndex < numTransmitterOptions) {
              // Toggle the option
              toggleTransmitterOption(optionIndex);
            }
          }
        }
        // Check for scroll up (top area)
        else if (touchY < 50 && touchY >= 30 && scrollOffset > 0) {
          scrollOffset--;
          drawTransmitterSetupScreen();
        }
        // Check for scroll down (bottom area, above action buttons)
        else if (touchY >= actionButtonsY - 10 && touchY < actionButtonsY && 
                scrollOffset + 4 < numTransmitterOptions) {
          scrollOffset++;
          drawTransmitterSetupScreen();
        }
      }
    }
    delay(10);
  }

  // Handle WiFi client in AP mode
  if (currentScreen == WIFI_CONFIG_SCREEN) {
    wifiManager.handleClient();
  }

  // Periodically check WiFi status
  unsigned long currentTime = millis();
  if (currentTime - lastWifiCheck > WIFI_CHECK_INTERVAL) {
    if (wifiManager.isConfigured() && !wifiManager.isAPMode()) {
      if (WiFi.status() != WL_CONNECTED) {
        bool connected = wifiManager.connect();
        Serial.println(connected ? "Reconnected to WiFi" : "Failed to reconnect to WiFi");
        if (currentScreen == MAIN_SCREEN) {
          drawMainScreen();
        }
      }
    }
    lastWifiCheck = currentTime;
  }
}

void drawStartupAnimation() {
  // Clear screen with white
  tft.fillScreen(TFT_WHITE);
  
  // Draw title
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawCentreString("SOUND METER", tft.width()/2, 30, 4);
  
  // Initialize progress bar
  const int barWidth = 200;
  const int barHeight = 20;
  const int barX = (tft.width() - barWidth) / 2;
  const int barY = tft.height() / 2 - 20;
  const int statusY = barY + 40;
  
  tft.drawRect(barX, barY, barWidth, barHeight, TFT_BLACK);
  
  // Status text
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  
  // Initial status
  String status = "Initializing...";
  int progress = 0;
  
  // Initial connection attempt
  tft.drawCentreString(status, tft.width()/2, statusY, 2);
  delay(500);
  
  // Check WiFi configuration and connection status
  if (WiFi.status() == WL_CONNECTED) {
    // Already connected
    progress = 60;
    status = "Connected: " + WiFi.SSID();
    tft.fillRect(0, statusY - 15, tft.width(), 40, TFT_WHITE);
    tft.drawCentreString(status, tft.width()/2, statusY, 2);
    tft.fillRect(barX + 2, barY + 2, map(progress, 0, 100, 0, barWidth - 4), barHeight - 4, TFT_BLUE);
  } 
  else if (wifiManager.isConfigured()) {
    status = "Connecting to WiFi...";
    tft.fillRect(0, statusY - 15, tft.width(), 40, TFT_WHITE);
    tft.drawCentreString(status, tft.width()/2, statusY, 2);
    
    // Attempt to connect
    if (wifiManager.connect()) {
      progress = 40;
      status = "Connected: " + WiFi.SSID();
      tft.fillRect(barX + 2, barY + 2, map(progress, 0, 100, 0, barWidth - 4), barHeight - 4, TFT_BLUE);
      tft.fillRect(0, statusY - 15, tft.width(), 40, TFT_WHITE);
      tft.drawCentreString(status, tft.width()/2, statusY, 2);
      
      // Show IP address
      delay(1000);
      status = "IP: " + WiFi.localIP().toString();
      tft.fillRect(0, statusY - 15, tft.width(), 40, TFT_WHITE);
      tft.drawCentreString(status, tft.width()/2, statusY, 2);
      
      // Test connection and check production state
      progress = 60;
      tft.fillRect(barX + 2, barY + 2, map(progress, 0, 100, 0, barWidth - 4), barHeight - 4, TFT_BLUE);
      status = "Testing connection...";
      tft.fillRect(0, statusY - 15, tft.width(), 40, TFT_WHITE);
      tft.drawCentreString(status, tft.width()/2, statusY, 2);
      
      // Fetch initial data and check production state
      if (fetchDecibelValue()) {
        progress = 70;
        status = "Checking production state...";
        tft.fillRect(barX + 2, barY + 2, map(progress, 0, 100, 0, barWidth - 4), barHeight - 4, TFT_BLUE);
        tft.fillRect(0, statusY - 15, tft.width(), 40, TFT_WHITE);
        tft.drawCentreString(status, tft.width()/2, statusY, 2);
        
        // Check production state
        isProductionMode = checkProductionState();
        progress = 80;
        status = isProductionMode ? "Production mode" : "Development mode";
      } else {
        progress = 80;
        status = "Connection test failed";
      }
    } else {
      progress = 40;
      status = "WiFi connection failed";
    }
  } else if (wifiManager.isAPMode()) {
    progress = 40;
    status = "AP Mode: CYD_Config";
  } else {
    progress = 40;
    status = "No WiFi configuration";
  }
  
  // Update display with final status
  tft.fillRect(barX + 2, barY + 2, map(progress, 0, 100, 0, barWidth - 4), barHeight - 4, TFT_BLUE);
  tft.fillRect(0, statusY - 15, tft.width(), 40, TFT_WHITE);
  tft.drawCentreString(status, tft.width()/2, statusY, 2);
  
  // Show next update time if connected
  if (isWiFiConnected()) {
    int timeLeft = DECIBEL_UPDATE_INTERVAL / 1000;
    tft.drawCentreString("Next update in: " + String(timeLeft) + "s", 
                        tft.width()/2, statusY + 20, 2);
  }
  delay(1000);
  tft.fillScreen(TFT_WHITE);
  
  // Animation parameters
  const int centerX = tft.width() / 2;
  const int centerY = tft.height() / 2;
  const int maxSize = 6;  // Maximum text size
  const int steps = 10;   // Number of animation steps
  
  // Pre-calculate text width for centering (approximate)
  const int charWidth = 6;  // Approximate width of a character in pixels at size 1
  const int textWidth = 5 * charWidth;  // "Done!" is 5 characters
  
  // Animation loop
  for (int i = 1; i <= steps; i++) {
    float progress = (float)i / steps;
    int currentSize = 1 + (maxSize - 1) * progress;  // Scale from 1 to maxSize
    
    // Calculate text position (approximate centering)
    int textW = textWidth * currentSize;
    int textH = 8 * currentSize;  // Approximate height
    int x = centerX - (textW / 2);
    int y = centerY - (textH / 2);
    
    // Clear previous frame (slightly larger area to prevent artifacts)
    int clearSize = maxSize * charWidth * 2;
    tft.fillRect(centerX - clearSize/2, centerY - clearSize/2, 
                clearSize, clearSize, TFT_WHITE);
    
    // Draw the text with current size
    tft.setTextSize(currentSize);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.setCursor(x, y);
    tft.print("Done!");
    
    delay(50);  // Animation speed
  }
  
  // Clear to white
  tft.fillScreen(TFT_WHITE);
  
  // Reset text size to default (1) for the rest of the application
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
}

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  while (!Serial); // Wait for serial to initialize
  
  Serial.println("\n=== Device Starting ===");
  
  // Initialize TFT display first
  tft.init();
  tft.setRotation(1); // Landscape mode
  tft.fillScreen(TFT_WHITE);
  
  // Display startup animation
  displayStartupAnimation();
  
  // Initialize GPIO0 button
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  
  // Initial screen while booting
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawCentreString("Booting...", tft.width()/2, tft.height()/2, 2);
  tft.drawCentreString("Please wait", tft.width()/2, tft.height()/2 + 20, 2);
  tft.setRotation(1); // Landscape mode
  tft.fillScreen(TFT_BLACK);
  
  // Initial production mode state (will be updated after WiFi connects)
  isProductionMode = false;
  
  // Initial screen draw
  drawStartupAnimation();
  drawMainScreen();
  
  // Schedule first theft check after a short delay
  lastTheftCheck = millis() - THEFT_CHECK_INTERVAL + INITIAL_CHECK_DELAY;
  Serial.println("Scheduled first theft check in 5 seconds...");
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  if (!ts.begin(mySpi)) {
    Serial.println("Failed to initialize touch controller!");
    while (1); // Halt if touch controller fails
  }
  
  // Show connection status
  //tft.fillScreen(TFT_BLACK);
  //tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawCentreString("Connecting to WiFi...", tft.width()/2, 50, 2);
  
  // Try to connect to WiFi if configured
 /* if (wifiManager.isConfigured()) {
    if (wifiManager.connect()) {
      tft.drawCentreString("Connected to WiFi!", tft.width()/2, 80, 2);
      tft.drawCentreString(WiFi.localIP().toString(), tft.width()/2, 110, 2);
      Serial.println("Connected to WiFi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      tft.drawCentreString("WiFi Connect Failed", tft.width()/2, 80, 2);
      Serial.println("WiFi connection failed");
    }
  } else {
    tft.drawCentreString("No WiFi Config", tft.width()/2, 80, 2);
    tft.drawCentreString("Use settings to configure", tft.width()/2, 110, 2);
    Serial.println("No WiFi configuration found");
  }
*/
  // Short delay to show status

  
  // Draw the initial screen
  drawMainScreen();
  
  // Initialize last update times
  lastWifiCheck = millis();
  lastDecibelUpdate = 0; // Force immediate update
  
  Serial.println("Setup complete - Ready for input");
}