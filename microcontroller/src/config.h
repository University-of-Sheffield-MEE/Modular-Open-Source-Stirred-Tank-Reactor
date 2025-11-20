#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

StaticJsonDocument<512> config;

const char* configSpiffsError = "Spiffs Error";
const char* configFileNotFound = "File not found";

const char* loadConfigFile() {
  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS Mount Failed, attempting to format...");
    SPIFFS.format();  // Format the filesystem

    // Attempt to mount again after formatting
    if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS Mount Failed after formatting");
      return configSpiffsError;
    }
  }

  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    return configFileNotFound;
  }
  DeserializationError error = deserializeJson(config, configFile);
  if (error) {
    Serial.println("DeserializationError error = deserializeJson(config, configFile)");
    return error.c_str();
  }
  return NULL;
}

const char* getConfigValue(const char* key) {
  return config[key];
}
const int getConfigIntValue(const char* key) {
  return config[key];
}
const float getConfigFloatValue(const char* key, float fallback = 0) {
  return config[key] | fallback;
}