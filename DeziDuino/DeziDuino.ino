/*
Mein C++ tut weh
*/




















#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include <HTTPClient.h>
#include "creds.h"
#include <TaskScheduler.h>
#include <driver/i2s.h>
#include <math.h>
#include <Preferences.h>

// AP Mode settings
#define AP_MODE_PIN 19
#define AP_SSID "DeziDuino_Config"

// Preferences namespace for storing WiFi credentials
Preferences preferences;

WebServer server(80);
bool apMode = false;

// INMP441 I2S Microphone configuration
#define I2S_WS 13  // LRCLK
#define I2S_SD 12  // DOUT
#define I2S_SCK 20 // BCLK
#define I2S_PORT I2S_NUM_0

// I2S configuration
const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// I2S pin configuration
const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD};

// Audio buffer and FFT setup
#define SAMPLE_BUFFER_SIZE 1024
#define SAMPLE_RATE 44100
int32_t samples[SAMPLE_BUFFER_SIZE];
float fft_input[SAMPLE_BUFFER_SIZE];
float fft_output[SAMPLE_BUFFER_SIZE];
float a_weighting[32]; // A-weighting filter coefficients for 1/3 octave bands
float db_value = 0;  // Will hold the calculated dBA value

// Initialize A-weighting filter coefficients (simplified 1/3 octave band approximation)
const float a_weighting_freqs[] = {25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 
                                  400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 
                                  4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};
const float a_weighting_db[] = {-44.7, -39.4, -34.6, -30.2, -26.2, -22.5, -19.1, -16.1, -13.4, 
                              -10.9, -8.6, -6.6, -4.8, -3.2, -1.9, -0.8, 0.0, 0.6, 1.0, 
                              1.2, 1.3, 1.2, 1.0, 0.5, -0.1, -1.1, -2.5, -4.3, -6.6, -9.3};

// Apply A-weighting to a frequency band
float applyAWeighting(float freq, float amplitude) {
    float ra = (12194.0f * 12194.0f * freq * freq * freq * freq) / 
              ((freq * freq + 20.6f * 20.6f) * 
               (freq * freq + 107.7f * 107.7f) * 
               (freq * freq + 737.9f * 737.9f) * 
               sqrtf(freq * freq + 12194.0f * 12194.0f));
    return ra * amplitude;
}

// Simple FFT implementation (Cooley-Tukey algorithm)
void fft(float* x, float* y, int n) {
    int i, j, k, n1, n2, a;
    float c, s, t1, t2;
    
    // Bit-reverse
    j = 0;
    for (i = 0; i < n - 1; i++) {
        if (i < j) {
            t1 = x[i];
            x[i] = x[j];
            x[j] = t1;
            t1 = y[i];
            y[i] = y[j];
            y[j] = t1;
        }
        k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }
    
    // FFT
    n1 = 0;
    n2 = 1;
    
    for (i = 0; i < log2(n); i++) {
        n1 = n2;
        n2 <<= 1;
        a = 0;
        
        for (j = 0; j < n1; j++) {
            c = cos(-2 * M_PI * j / n2);
            s = sin(-2 * M_PI * j / n2);
            
            for (k = j; k < n; k += n2) {
                t1 = c * x[k + n1] - s * y[k + n1];
                t2 = s * x[k + n1] + c * y[k + n1];
                x[k + n1] = x[k] - t1;
                y[k + n1] = y[k] - t2;
                x[k] = x[k] + t1;
                y[k] = y[k] + t2;
            }
            a += 1 << (i + 1);
        }
    }
}

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;     // Central European Time (CET) = UTC+1
const int   daylightOffset_sec = 3600; // Central European Summer Time (CEST) = UTC+2

// Function to check if DST is in effect (for Berlin timezone)
bool isDST(int year, int month, int day, int hour, int weekday) {
  // DST starts on last Sunday of March at 2:00 AM
  if (month < 3 || month > 10) return false;
  if (month > 3 && month < 10) return true;
  
  int previousSunday = day - weekday;
  
  // In March, we are DST if previous Sunday was >= 25 (last Sunday of March)
  if (month == 3) {
    if (previousSunday >= 25) {
      // After last Sunday in March
      return true;
    } else if (previousSunday < 25) {
      // Before last Sunday in March
      return false;
    } else {
      // On the last Sunday in March, check if 2:00 AM has passed
      return (hour >= 2);
    }
  }
  
  // In October, we are DST if previous Sunday was < 25 (last Sunday of October)
  if (month == 10) {
    if (previousSunday < 25) {
      // Before last Sunday in October
      return true;
    } else if (previousSunday > 25) {
      // After last Sunday in October
      return false;
    } else {
      // On the last Sunday in October, check if 3:00 AM has passed
      return (hour < 3);
    }
  }
  
  return false; // Shouldn't reach here
}

int buzzer = 15;
// Store last 10 measurements
const int MAX_MEASUREMENTS = 10;
int measurementCount = 0;

// Webhook URLs
const char* realtimeDbUrl = "https://n8n.zvdxc.is-a.dev/webhook/realtimedb";

// Task scheduler for async operations
Scheduler runner;

// Function to send data to realtime database asynchronously
void sendToRealtimeDb(JsonObject measurement) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot send to realtime DB");
    return;
  }

  // Create a new HTTP client for this request
  HTTPClient* http = new HTTPClient();
  http->begin(realtimeDbUrl);
  http->addHeader("Content-Type", "application/json");
  
  // Create a JSON document for the single measurement
  DynamicJsonDocument doc(512);
  doc.set(measurement);
  
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  // Send the request in a non-blocking way
  http->setTimeout(5000); // 5 second timeout
  int httpResponseCode = http->POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    String response = http->getString();
    Serial.printf("Realtime DB response: %d - %s\n", httpResponseCode, response.c_str());
  } else {
    Serial.printf("Error sending to realtime DB: %s\n", http->errorToString(httpResponseCode).c_str());
  }
  
  // Clean up
  http->end();
  delete http;
}
StaticJsonDocument<512> doc;  // Adjust size if needed
JsonArray measurements = doc.to<JsonArray>();
// Function to get current Unix timestamp
unsigned long getUnixTime() {
  time_t now;
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return 0;
  }
  time(&now);
  return now;
}

// Function to read audio samples and calculate dBA
bool readAudioSamples() {
  static float y[SAMPLE_BUFFER_SIZE] = {0};
  static float last_db = 0;
  
  size_t bytes_read = 0;
  esp_err_t result = i2s_read(I2S_PORT, 
                             (void*)samples, 
                             SAMPLE_BUFFER_SIZE * sizeof(int32_t), 
                             &bytes_read, 
                             portMAX_DELAY);
  
  if (result == ESP_OK) {
    int samples_read = bytes_read / sizeof(int32_t);
    
    // Convert samples to float and apply window function (Hann window)
    float sum_squares = 0;
    for (int i = 0; i < samples_read; i++) {
      // Convert to float and apply Hann window
      float sample = (float)(samples[i] >> 14); // Convert to 18-bit signed
      float window = 0.5f * (1.0f - cosf(2 * M_PI * i / (samples_read - 1)));
      fft_input[i] = sample * window;
      y[i] = 0; // Initialize imaginary part to 0
    }
    
    // Perform FFT
    fft(fft_input, y, samples_read);
    
    // Calculate power spectrum and apply A-weighting
    for (int i = 0; i < samples_read/2; i++) {
      // Calculate frequency of this bin
      float freq = (float)i * SAMPLE_RATE / samples_read;
      
      // Calculate magnitude
      float magnitude = sqrtf(fft_input[i] * fft_input[i] + y[i] * y[i]);
      
      // Apply A-weighting
      float weighted_magnitude = applyAWeighting(freq, magnitude);
      
      // Sum weighted power (squared magnitude)
      sum_squares += weighted_magnitude * weighted_magnitude;
    }
    
    // Calculate RMS and convert to dB
    if (samples_read > 0) {
      float rms = sqrtf(sum_squares / (samples_read/2));
      
      // Convert to dB with a reference level that gives reasonable values
      // Calibrated to match phone measurements
      float reference = 10000.0f; // Further adjusted to better match phone readings
      db_value = 20.0f * log10f(rms / reference) + 94.0f; // 94dB SPL reference
      
      // Apply some smoothing to the dB value
      db_value = 0.2f * db_value + 0.8f * last_db;
      last_db = db_value;
      
      return true;
    }
  }
  return false;
}

// Function to handle root URL
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>DeziDuino WiFi Setup</h1>";
  html += "<form method='post' action='/save'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Password: <input type='password' name='password'><br>";
  html += "<input type='submit' value='Save and Restart'>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Function to handle saving credentials
void handleSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  if (ssid.length() > 0 && password.length() > 0) {
    // Save to Preferences
    preferences.begin("wifi", false); // false for read/write mode
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    
    server.send(200, "text/plain", "Credentials saved. Restarting...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Error: SSID and password cannot be empty");
  }
}

// Function to enter AP mode
void startAPMode() {
  Serial.begin(115200);
  Serial.println("Starting AP Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID); // No password
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  
  while (true) {
    server.handleClient();
    // Beep every 2 seconds
    static unsigned long lastBeep = 0;
    if (millis() - lastBeep >= 2000) {
      tone(buzzer, 1000);
      delay(100);
      noTone(buzzer);
      lastBeep = millis();
    }
  }
}

void setup() {
  // Initialize Preferences
  preferences.begin("wifi", true); // true for read-only mode initially
  
  // Check if AP mode pin is grounded
  pinMode(AP_MODE_PIN, INPUT_PULLUP);
  if (digitalRead(AP_MODE_PIN) == LOW) {
    preferences.end(); // Close read-only mode before starting AP mode
    startAPMode();
  }
  
  // Initialize task scheduler
  runner.init();
  Serial.println("TaskScheduler initialized");
  // Initialize I2S
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  
  // Set I2S clock and pin configuration
  i2s_set_clk(I2S_PORT, 44100, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_MONO);

  pinMode(buzzer, OUTPUT);

  // Startup beep
  tone(buzzer, 500);
  delay(500);
  noTone(buzzer);
  delay(100);
  tone(buzzer, 1000);
  delay(500);
  noTone(buzzer);

  Serial.begin(115200);
  Serial.println("Welcome");
  Serial.println("DeziDuino with INMP441 is starting");
  
  tone(buzzer, 500);
  delay(100);
  noTone(buzzer);

  // Initialize WiFi with timeout
  Serial.println("Connecting to WiFi...");
  
  // Try to read credentials from Preferences
  String storedSSID = preferences.getString("ssid", "");
  String storedPassword = preferences.getString("password", "");
  preferences.end(); // Close preferences after reading
  
  // Use stored credentials if available, otherwise use hardcoded ones
  const char* ssidToUse = (!storedSSID.isEmpty()) ? storedSSID.c_str() : WIFI_SSID;
  const char* passwordToUse = (!storedPassword.isEmpty()) ? storedPassword.c_str() : WIFI_PASSWORD;
  
  WiFi.begin(ssidToUse, passwordToUse);
  
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED) {
    // Check if timeout occurred (20 seconds)
    if (millis() - startAttemptTime > 10000) {
      Serial.println("\nFailed to connect to WiFi. Rebooting...");
	tone(buzzer, 500);
delay(1000);
noTone(buzzer);
tone(buzzer, 250);
delay(1000);
noTone(buzzer);
tone(buzzer, 100);
delay(500);
      ESP.restart();
    }
    
    delay(500);
    Serial.print(".");
	tone(buzzer, 500);
delay(100);
noTone(buzzer);

  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
	tone(buzzer, 3000);
delay(700);
	delay(700);
	noTone(buzzer);
  // Initialize time after WiFi is connected
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait for time to be set
  Serial.println("Synchronizing time with NTP server...");
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 24 * 3600 && retry++ < 5000) {
	Serial.print(".");
    now = time(nullptr);
    tone(buzzer, 250);
	delay(100);
	noTone(buzzer);
delay(50);

  }
  
  // Print current time
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    String timeString = String(timeStringBuff);
    
    // Check DST status
    bool dstActive = isDST(
      timeinfo.tm_year + 1900, 
      timeinfo.tm_mon + 1, 
      timeinfo.tm_mday, 
      timeinfo.tm_hour,
      timeinfo.tm_wday
    );
    noTone(buzzer);
    Serial.println("\nTime synchronized: " + timeString);
    Serial.println("Timezone: Europe/Berlin (" + String(dstActive ? "CEST, DST" : "CET") + ")");
	tone(buzzer, 5000);
	delay(200);
	noTone(buzzer);
} else {
noTone(buzzer);
    Serial.println("\nFailed to obtain time! Using system time.");
	tone(buzzer, 250);
	delay(5000);
	noTone(buzzer);
}

}
void loop() {
  static unsigned long lastReadTime = 0;
  static float max_db = 0;
  static unsigned long lastMaxReset = millis();
  static unsigned long lastMeasurementTime = 0;
  
  // Read audio samples
  if (readAudioSamples()) {
    // Update max dB value
    if (db_value > max_db) {
      max_db = db_value;
    }
    
    // Print current and max dB every second for debugging
    if (millis() - lastReadTime > 1) {
      Serial.print("Current dB: ");
      Serial.print(db_value);
      Serial.print(" dB, Max dB: ");
      Serial.print(max_db);
      Serial.println(" dB");
      lastReadTime = millis();
    }
    
    // Take measurement every 5 seconds
    if (millis() - lastMeasurementTime > 100) {
      // Create new measurement
      JsonObject measurement = measurements.createNestedObject();
      measurement["timestamp"] = getUnixTime();
      measurement["decibel"] = db_value;
      
      // Send to realtime database asynchronously
      sendToRealtimeDb(measurement);
      
      // Add human-readable time if available
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        char timeStringBuff[20];
        strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
        measurement["datetime"] = String(timeStringBuff);
        measurement["timezone"] = "Europe/Berlin";
        measurement["is_dst"] = isDST(
          timeinfo.tm_year + 1900, 
          timeinfo.tm_mon + 1, 
          timeinfo.tm_mday, 
          timeinfo.tm_hour,
          timeinfo.tm_wday
        );
      }
      
          // Keep only the last 10 measurements
      if (measurements.size() > MAX_MEASUREMENTS) {
        measurements.remove(0);
      }
      
      // Print all measurements in JSON format
      String jsonOutput;
      serializeJsonPretty(doc, jsonOutput);
      Serial.println("All measurements:");
      Serial.println(jsonOutput);
      
      // Send to webhook every 10 measurements (when we have a full buffer)
      static int measurementCount = 0;
      measurementCount++;
      
      if (measurementCount >= 10) {
        measurementCount = 0;
        if (WiFi.status() == WL_CONNECTED) {
          if (sendToWebhook()) {
            Serial.println("Data sent to webhook successfully");
            // Clear measurements after successful send
            doc.clear();
            measurements = doc.to<JsonArray>();
          } else {
            // Keep measurements for next attempt
            measurementCount = 9; // Will be incremented to 10 in next iteration
          }
        }
      }
      
      lastMeasurementTime = millis();
    }
  }
  
  // Reset max value every minute
  if (millis() - lastMaxReset > 60000) {
    max_db = 0;
    lastMaxReset = millis();
    Serial.println("Resetting max dB value");
  }
  
  // Execute any pending tasks
  runner.execute();
  delay(10); // Small delay to prevent watchdog issues
}

bool sendToWebhook() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, cannot send data");
    return false;
  }

  HTTPClient http;
  http.begin(webhookUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Convert measurements to JSON string
  String jsonPayload;
  serializeJson(doc, jsonPayload);
  
  Serial.println("Sending data to webhook...");
  Serial.println("Payload: " + jsonPayload);
  
  // Try up to MAX_RETRIES times
  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.printf("Webhook response (attempt %d/%d): %d - %s\n", 
                   attempt, MAX_RETRIES, httpResponseCode, response.c_str());
      
      if (httpResponseCode == HTTP_CODE_OK || httpResponseCode == HTTP_CODE_CREATED) {
        http.end();
        return true;
      }
    } else {
      Serial.printf("Error sending to webhook (attempt %d/%d): %s\n", 
                   attempt, MAX_RETRIES, http.errorToString(httpResponseCode).c_str());
    }
    
    if (attempt < MAX_RETRIES) {
      delay(RETRY_DELAY);
    }
  }
  
  http.end();
  return false;
}