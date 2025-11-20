#include <Arduino.h>
#include <WiFi.h>
#include <esp_wpa2.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>

#include "events.h"

class EduroamTask : public Task, public TSEvents::EventHandler {
 public:
  EduroamTask(Scheduler& s, TSEvents::EventBus& e, const char* _user, const char* _pass)
      : Task(1000 * TASK_MILLISECOND, TASK_FOREVER, &s, false),
        TSEvents::EventHandler(&s, &e) {
    user = _user;
    pass = _pass;
  }

  bool OnEnable() {
    WiFi.disconnect(true);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    delay(200);
    connect();
    return true;
  }

  bool Callback() {
    switch (state) {
      case CONNECTING:
        switch (WiFi.status()) {
          case WL_CONNECTED:
            // Connecting complete
            dispatch(WIFI_CONNECTED);
            state = CONNECTED;
            setInterval(1 * TASK_SECOND);
            break;
          case WL_NO_SSID_AVAIL:
            // Connecting failed
            dispatch(WIFI_CONNECT_FAILED, "No SSID Available");
            state = DISCONNECTED;
            setInterval(1 * TASK_MINUTE);
            break;
          case WL_CONNECT_FAILED:
            // Connecting failed
            dispatch(WIFI_CONNECT_FAILED, "Failed");
            state = DISCONNECTED;
            setInterval(1 * TASK_MINUTE);
            break;
          default:
            if (millis() - connectTime > 30e3) {
              // Connection timed out
              dispatch(WIFI_CONNECT_FAILED, "Connection timeout");
              state = DISCONNECTED;
              setInterval(1 * TASK_MINUTE);
            }
        }
        break;
      case CONNECTED:
        if (WiFi.status() != WL_CONNECTED) {
          // Connection lost
          dispatch(WIFI_DISCONNECTED);
          state = DISCONNECTED;
          setInterval(1 * TASK_MINUTE);
          connect();
          break;
        }
        break;
      case DISCONNECTED:
        connect();
        break;
    }
    return true;
  }

  bool connect() {
    int apIndex = findAPIndex("eduroam");
    if (apIndex == -1) {
      dispatch(WIFI_CONNECT_FAILED, "SSID not detectable");
      return false;
    }
    int32_t channel = WiFi.channel(apIndex);
    uint8_t* bssid = WiFi.BSSID(apIndex);

    WiFi.begin("eduroam", WPA2_AUTH_PEAP, user, user, pass, NULL, NULL, NULL, channel, bssid, true);
    state = CONNECTING;
    connectTime = millis();
    setInterval(100 * TASK_MILLISECOND);
    dispatch(WIFI_CONNECTING);
    return true;
  }

  void disconnect() {
    WiFi.disconnect(true);
    disable();
    if (state != DISCONNECTED) {
      dispatch(WIFI_DISCONNECTED);
      state = DISCONNECTED;
    }
  }

  void HandleEvent(TSEvents::Event event) {
  }

 private:
  int findAPIndex(String ssid) {
    int index = -1;
    int32_t rssi = 0;
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == ssid) {
        int32_t newRSSI = WiFi.RSSI(i);
        if (index == -1 || newRSSI > rssi) {
          index = i;
          rssi = newRSSI;
        }
      }
    }
    return index;
  }

  enum State {
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
  };

  unsigned long connectTime;
  State state;
  const char* user;
  const char* pass;
};