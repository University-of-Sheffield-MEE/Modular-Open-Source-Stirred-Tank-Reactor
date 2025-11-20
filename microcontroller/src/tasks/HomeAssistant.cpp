#include <Arduino.h>
#include <WiFi.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <ArduinoJson.h>
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>

#include "events.h"
#include "tasks/MQTT.cpp"
#include "version.h"

typedef struct {
  const char* id;
  const char* name;
  const char* deviceClass;
  const char* unit;
  EventType eventId;
  float value;
  uint16_t valueCount;
} hassSensor;

class HomeAssistantTask : public Task, public TSEvents::EventHandler {
 public:
  HomeAssistantTask(Scheduler& s, TSEvents::EventBus& e, MQTTTask* _mqtt, const char* _deviceName, hassSensor* _sensors, int _sensorCount, unsigned long interval = 5 * TASK_SECOND)
      : Task(interval, TASK_FOREVER, &s, false),
        TSEvents::EventHandler(&s, &e) {
    mqtt = _mqtt;
    deviceName = _deviceName;
    sensors = _sensors;
    sensorCount = _sensorCount;
  }

  bool OnEnable() {
    return mqtt->isConnected();
  }

  bool Callback() {
    if (!discoveryMessageSent) {
      discoveryMessageSent = sendDiscoveryMessage();
      if (!discoveryMessageSent) {
        return true;
      }
    }
    sendDataMessage();
    return true;
  }

  void HandleEvent(TSEvents::Event event) {
    switch (event.id) {
      case MQTT_SERVER_CONNECTED:
        enable();
        break;
      case MQTT_SERVER_DISCONNECTED:
        disable();
        break;
      default: {
        for (int i = 0; i < sensorCount; i++) {
          hassSensor sensor = sensors[i];
          if (sensor.eventId == event.id) {
            if (event.id == ENCODER_1_DATA) {
              float value = *(double*)event.data;
              setValue(sensor.id, *(double*)event.data);
            } else {
              float value = *(float*)event.data;
              setValue(sensor.id, *(float*)event.data);
            }
          }
        }
      }
    }
  }

  bool setValue(const char* id, float value) {
    for (int i = 0; i < sensorCount; i++) {
      if (strcmp(sensors[i].id, id) == 0) {
        // Maintain a running average of the passed values
        sensors[i].value = ((float)sensors[i].valueCount * sensors[i].value + value) / (sensors[i].valueCount + 1);
        sensors[i].valueCount++;
        return true;
      }
    }
    return false;
  }

  bool sendDiscoveryMessage() {
    char statusTopic[64];
    char discoveryTopic[64];
    char valTpl[64];
    char device_id[16];
    char id[64];
    char name[64];
    getStatusTopic(statusTopic);
    getUniqueId(device_id);

    for (int i = 0; i < sensorCount; i++) {
      hassSensor sensor = sensors[i];
      getDiscoveryTopic(discoveryTopic, sensor.id);
      sprintf(id, "%s_%s", deviceName, sensor.id);
      sprintf(name, "%s %s", deviceName, sensor.name);
      sprintf(valTpl, "{{ value_json.%s | is_defined }}", sensor.id);

      payload.clear();
      payload["name"] = name;
      payload["uniq_id"] = id;
      payload["stat_t"] = statusTopic;
      payload["dev_cla"] = sensor.deviceClass;
      payload["val_tpl"] = valTpl;
      payload["unit_of_meas"] = sensor.unit;
      payload["force_update"] = true;

      JsonObject device = payload.createNestedObject("device");
      device["name"] = deviceName;
      device["sw_version"] = VERSION;
      device["manufacturer"] = "M5Stack";
      device["model"] = "M5Tough";
      device["suggested_area"] = "Analytics Lab";

      JsonArray identifiers = device.createNestedArray("identifiers");
      identifiers.add(deviceName);

      serializeJson(payload, message);
      bool ok = mqtt->sendMessage(discoveryTopic, message);
      if (!ok) {
        return false;
      }
    }
    return true;
  }

  bool sendDataMessage() {
    char statusTopic[64];
    getStatusTopic(statusTopic);

    payload.clear();
    bool anyUpdate = false;
    for (int i = 0; i < sensorCount; i++) {
      hassSensor sensor = sensors[i];
      if (sensor.valueCount > 0) {
        payload[sensor.id] = sensor.value;
        anyUpdate = true;
      }
    }

    if (!anyUpdate) {
      return true;
    }

    serializeJson(payload, message);
    bool ok = mqtt->sendMessage(statusTopic, message);
    if (ok) {
      for (int i = 0; i < sensorCount; i++) {
        hassSensor sensor = sensors[i];
        sensor.valueCount = 0;
        sensors[i] = sensor;
      }
    }
    return ok;
  }

 private:
  void getDiscoveryTopic(char* topic, const char* sensorId) {
    sprintf(topic, "homeassistant/sensor/%s/%s/config", deviceName, sensorId);
  }

  void getStatusTopic(char* topic) {
    sprintf(topic, "homeassistant/sensor/%s/state", deviceName);
  }

  void getUniqueId(char* id) {
    byte mac[6];
    WiFi.macAddress(mac);
    sprintf(id, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  MQTTTask* mqtt;
  const char* deviceName;
  hassSensor* sensors;
  int sensorCount;
  StaticJsonDocument<512> payload;
  char message[512];
  bool discoveryMessageSent = false;
};