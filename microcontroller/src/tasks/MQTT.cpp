#pragma once

#include <Arduino.h>
#include <WiFi.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <PubSubClient.h>
#include <TaskSchedulerDeclarations.h>
#include <WiFiUdp.h>

#include "events.h"

class MQTTTask : public Task, public TSEvents::EventHandler {
 public:
  MQTTTask(Scheduler& s, TSEvents::EventBus& e, const char* domain, const int port, const char* _id)
      : Task(1 * TASK_SECOND, TASK_FOREVER, &s, false),
        TSEvents::EventHandler(&s, &e) {
    wifiClient = new WiFiClient();
    client = new PubSubClient(*wifiClient);
    client->setServer(domain, port);
    client->setBufferSize(1024);
    id = _id;
    state = DISCONNECTED;
  }

  bool OnEnable() {
    return WiFi.status() == WL_CONNECTED;
  }

  bool Callback() {
    switch (state) {
      case CONNECTED:
        if (client->connected()) {
          client->loop();
        } else {
          // connection lost
          state = DISCONNECTED;
          dispatch(MQTT_SERVER_DISCONNECTED);
          if (WiFi.status() == WL_CONNECTED) {
            connect();
          }
        }
        break;
      case DISCONNECTED:
        if (WiFi.status() == WL_CONNECTED) {
          connect();
        }
    }
    return true;
  }

  void HandleEvent(TSEvents::Event event) {
    switch (event.id) {
      case WIFI_CONNECTED:
        enable();
        connect();
        break;
      case WIFI_DISCONNECTED:
        disable();
        if (state == CONNECTED) {
          state = DISCONNECTED;
          dispatch(MQTT_SERVER_DISCONNECTED);
        }
        break;
    }
  }

  bool connect() {
    enableIfNot();
    bool ok = client->connect(id);
    if (!ok) {
      dispatch(MQTT_SERVER_CONNECT_FAILED);
      return false;
    }
    state = CONNECTED;
    dispatch(MQTT_SERVER_CONNECTED);
    return true;
  }

  bool sendMessage(const char* topic, const char* payload) {
    if (state != CONNECTED) {
      return false;
    }
    return client->publish(topic, payload);
  }

  bool isConnected() {
    return state == CONNECTED;
  }

 private:
  enum State {
    CONNECTED,
    DISCONNECTED,
  };
  WiFiClient* wifiClient;
  PubSubClient* client;
  const char* id;
  State state;
};