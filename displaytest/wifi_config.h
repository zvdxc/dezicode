#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <string.h>
#include <nvs_flash.h>

// Namespace for preferences
#define PREF_NAMESPACE "wifi_config"

class WiFiManager {
private:
    WebServer* server = nullptr;
    bool apMode = false;
    static const int SSID_LEN = 32;
    static const int PASS_LEN = 64;
    
    struct WiFiConfig {
        char ssid[SSID_LEN] = {0};
        char password[PASS_LEN] = {0};
    } config;

    void handleRoot() {
        if (!server) return;
        
        String html = "<html><body>";
        html += "<h1>WiFi Configuration</h1>";
        html += "<form method='post' action='/save'>";
        html += "SSID: <input type='text' name='ssid'><br>";
        html += "Password: <input type='password' name='password'><br>";
        html += "<input type='submit' value='Save'>";
        html += "</form></body></html>";
        
        server->send(200, "text/html", html);
    }
    
    void handleSave() {
        if (!server) return;
        
        if (server->hasArg("ssid") && server->hasArg("password")) {
            strncpy(config.ssid, server->arg("ssid").c_str(), SSID_LEN - 1);
            config.ssid[SSID_LEN - 1] = '\0';
            
            strncpy(config.password, server->arg("password").c_str(), PASS_LEN - 1);
            config.password[PASS_LEN - 1] = '\0';
            
            saveConfig();
            server->send(200, "text/plain", "Configuration saved. Device will restart.");
            delay(1000);
            ESP.restart();
        } else {
            server->send(400, "text/plain", "Missing SSID or password");
        }
    }
    
    void loadConfig() {
        Preferences prefs;
        
        // Initialize NVS
        if (!prefs.begin(PREF_NAMESPACE, true)) {
            Serial.println("Failed to open preferences namespace");
            memset(&config, 0, sizeof(config));
            return;
        }
        
        // Read SSID and password
        size_t ssidLen = prefs.getBytes("ssid", config.ssid, SSID_LEN);
        size_t passLen = prefs.getBytes("pass", config.password, PASS_LEN);
        
        // Ensure null termination
        config.ssid[SSID_LEN - 1] = '\0';
        config.password[PASS_LEN - 1] = '\0';
        
        // Debug output
        Serial.println("Loaded config from NVS:");
        Serial.print("SSID: "); 
        if (ssidLen == 0 || config.ssid[0] == 0) {
            Serial.println("<not set>");
            memset(&config, 0, sizeof(config));
        } else {
            Serial.println(config.ssid);
        }
        
        prefs.end();
    }
    
    void saveConfig() {
        Serial.println("Saving config to NVS...");
        Serial.print("SSID: "); 
        Serial.println(config.ssid[0] ? config.ssid : "<none>");
        Serial.print("Password: "); 
        Serial.println(config.password[0] ? "[set]" : "<none>");
        
        Preferences prefs;
        if (!prefs.begin(PREF_NAMESPACE, false)) {
            Serial.println("Failed to open NVS for writing");
            return;
        }
        
        // Store SSID and password
        bool ssidSaved = prefs.putBytes("ssid", config.ssid, strnlen(config.ssid, SSID_LEN - 1) + 1) > 0;
        bool passSaved = prefs.putBytes("pass", config.password, strnlen(config.password, PASS_LEN - 1) + 1) > 0;
        
        // Close preferences (automatically commits)
        prefs.end();
        
        if (ssidSaved && passSaved) {
            Serial.println("Config saved to NVS successfully");
        } else {
            Serial.println("Failed to save config to NVS");
        }
    }
    
public:
    WiFiManager() {
        // Initialize Serial for debugging
        if (!Serial) {
            Serial.begin(115200);
            delay(1000); // Give time for Serial to initialize
        }
        
        Serial.println("\nWiFiManager initializing...");
        
        // Initialize config with zeros
        memset(&config, 0, sizeof(config));
        
        // Initialize NVS
        esp_err_t ret = nvs_flash_init();
        if (ret == 0x1010 || ret == 0x1102) {  // ESP_ERR_NVS_NO_FREE_PAGES or ESP_ERR_NVS_NEW_VERSION_FOUND
            // NVS partition was truncated and needs to be erased
            Serial.println("Erasing NVS partition...");
            nvs_flash_erase();
            ret = nvs_flash_init();
        }
        
        if (ret != 0) {
            Serial.println("Failed to initialize NVS");
        }
        
        // Load configuration
        loadConfig();
        
        // Debug output
        Serial.println("WiFiManager initialized");
        Serial.print("Configured SSID: ");
        if (config.ssid[0] == 0) {
            Serial.println("<none>");
        } else {
            Serial.println(config.ssid);
        }
    }
    
    ~WiFiManager() {
        if (server) {
            delete server;
            server = nullptr;
        }
    }
    
    bool connect() {
        if (config.ssid[0] == 0) {
            return false;
        }
        
        WiFi.mode(WIFI_STA);
        WiFi.begin(config.ssid, config.password);
        
        // Wait for connection
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            attempts++;
        }
        
        return WiFi.status() == WL_CONNECTED;
    }
    
    void startAP() {
        WiFi.mode(WIFI_AP);
        WiFi.softAP("CYD_Config", "");
        apMode = true;
        
        if (!server) {
            server = new WebServer(80);
            
            // Handle root URL
            server->on("/", HTTP_GET, [this]() {
                handleRoot();
            });
            
            // Handle form submission
            server->on("/save", HTTP_POST, [this]() {
                handleSave();
            });
            
            server->begin();
        }
    }
    
    void handleClient() {
        if (server) {
            server->handleClient();
        }
    }
    
    // No need for handleClient() with AsyncWebServer as it runs in the background
    
    bool isConfigured() const {
        return config.ssid[0] != 0;
    }
    
    bool isAPMode() const {
        return apMode;
    }
    
    bool isConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }
    
    String getStatus() const {
        if (apMode) return "AP Mode";
        return isConnected() ? WiFi.localIP().toString() : "No WiFi";
    }
};

#endif
