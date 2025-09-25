#include "ota/OtaService.h"

#include <ElegantOTA.h>
#include <Update.h>

#include <Arduino.h>

namespace {
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiRetryIntervalMs = 5000;
}

OtaService::OtaService()
    : server_(80) {}

OtaService::~OtaService() {
  if (serverStarted_) {
    server_.end();
  }
  if (wifiConnected_) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  }
}

bool OtaService::begin(const ConfigManager::Config &config) {
  if (active_) {
    return true;
  }
  if (!config.ota.enabled) {
    Serial.println("[OTA] Disabled via config");
    return false;
  }
  wifiConfig_ = config.wifi;
  wifiConfigValid_ = !wifiConfig_.ssid.empty();
  authEnabled_ = !config.ota.username.empty();
  if (authEnabled_) {
    username_ = config.ota.username;
    password_ = config.ota.password;
  } else {
    username_.clear();
    password_.clear();
  }
  if (!wifiConfigValid_) {
    Serial.println("[OTA] WiFi settings not provided");
    return false;
  }
  if (!connectWiFi(wifiConfig_)) {
    Serial.println("[OTA] WiFi connection failed");
    return false;
  }
  setupServer(config);
  active_ = true;
  Serial.printf("[OTA] Service started. http://%s/", WiFi.localIP().toString().c_str());
  if (!config.ota.username.empty()) {
    Serial.printf(" (user:%s)\n", config.ota.username.c_str());
  } else {
    Serial.println();
  }
  return true;
}

void OtaService::loop() {
  if (!active_) {
    return;
  }
  ElegantOTA.loop();
  if (wifiConnected_ && WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA] WiFi disconnected, attempting reconnect");
    wifiConnected_ = false;
  }
  if (!wifiConnected_ && wifiConfigValid_) {
    uint32_t now = millis();
    if (now - lastWifiAttemptMs_ >= kWifiRetryIntervalMs) {
      lastWifiAttemptMs_ = now;
      connectWiFi(wifiConfig_);
    }
  }
}

bool OtaService::connectWiFi(const ConfigManager::WifiConfig &wifiConfig) {
  if (wifiConfig.ssid.empty()) {
    Serial.println("[OTA] WiFi SSID not provided");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());
  Serial.printf("[OTA] Connecting to WiFi SSID: %s\n", wifiConfig.ssid.c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < kWifiConnectTimeoutMs) {
    delay(200);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA] WiFi connection timeout");
    return false;
  }
  wifiConnected_ = true;
  lastWifiAttemptMs_ = millis();
  Serial.printf("[OTA] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

void OtaService::setupServer(const ConfigManager::Config &config) {
  if (serverStarted_) {
    return;
  }
  hostname_ = config.system.name.empty() ? "isolation-sphere" : config.system.name;
  WiFi.setHostname(hostname_.c_str());

  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "text/plain",
                  "Isolation Sphere OTA endpoint\n/ota -> firmware upload\n/uploadfs -> LittleFS image upload");
  });

  if (!config.ota.username.empty()) {
    ElegantOTA.begin(&server_, config.ota.username.c_str(), config.ota.password.c_str());
  } else {
    ElegantOTA.begin(&server_);
  }
  ElegantOTA.setAutoReboot(false);
  ElegantOTA.onEnd([this](bool success) {
    if (success) {
      needsReboot_ = true;
    }
  });

  server_.on("/uploadfs", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (authEnabled_) {
      if (!request->authenticate(username_.c_str(), password_.c_str())) {
        return request->requestAuthentication();
      }
    }
    static const char kFormHtml[] =
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>LittleFS Upload</title>"
        "<style>body{font-family:sans-serif;margin:2rem;}form{margin-top:1rem;}input[type=file]{margin:0.5rem 0;}"
        "button{padding:0.5rem 1.2rem;}</style></head><body>"
        "<h1>LittleFS Upload</h1>"
        "<p>LittleFSのイメージファイル（.bin）を選択してアップロードしてください。</p>"
        "<form method=\"POST\" action=\"/uploadfs\" enctype=\"multipart/form-data\">"
        "<input type=\"file\" name=\"file\" accept=\".bin\" required>"
        "<div><button type=\"submit\">Upload</button></div>"
        "</form></body></html>";
    request->send(200, "text/html", kFormHtml);
  });

  server_.on(
      "/uploadfs", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        if (authEnabled_) {
          if (!request->authenticate(username_.c_str(), password_.c_str())) {
            return request->requestAuthentication();
          }
        }
        if (Update.hasError()) {
          request->send(500, "text/plain", "LittleFS update failed");
        } else {
          request->send(200, "text/plain", "LittleFS update successful, rebooting soon");
          needsReboot_ = true;
        }
      },
      [this](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len,
             bool final) {
        if (authEnabled_ && !request->authenticate(username_.c_str(), password_.c_str())) {
          return;
        }
        handleFsUpload(request, filename, index, data, len, final);
      });

  server_.begin();
  serverStarted_ = true;
}

void OtaService::handleFsUpload(AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data,
                                size_t len, bool final) {
  if (index == 0) {
    Serial.printf("[OTA] LittleFS upload start: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
      Update.printError(Serial);
    }
  }
  if (len) {
    if (Update.write(data, len) != len) {
      Update.printError(Serial);
    }
  }
  if (final) {
    if (!Update.end(true)) {
      Update.printError(Serial);
    } else {
      Serial.printf("[OTA] LittleFS upload finished (%s)\n", filename.c_str());
    }
  }
}
