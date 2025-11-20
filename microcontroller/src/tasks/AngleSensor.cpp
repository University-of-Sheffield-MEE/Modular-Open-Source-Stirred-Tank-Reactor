#include <Arduino.h>
#include <M5Tough.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>

#include <tasks/I2CHub.cpp>
#include <tasks/PortBHub.cpp>

#include "events.h"

// https://github.com/m5stack/M5Stack/blob/master/examples/Unit/ANGLE/ANGLE.ino

class AngleSensorTask : public Task, public TSEvents::EventEmitter {
 public:
  AngleSensorTask(Scheduler& s, TSEvents::EventBus& e, PortBHubTask* _portBHub, PortBChannel _port, EventType _event, unsigned long _interval = 1000 * TASK_MILLISECOND)
      : Task(_interval, TASK_FOREVER, &s, false),
        TSEvents::EventEmitter(&e) {
    portBHub = _portBHub;
    port = _port;
    event = _event;
  }

  bool OnEnable() {
    bool ok = portBHub->checkConnection();
    if (!ok) {
      return true;
    }
    return true;
  }

  bool Callback() {
    uint16_t value = portBHub->analogRead(port);
    if (abs(value - lastSensorValue) > 10) {  // debounce
      lastSensorValue = value;
      value = 4096 - value;  // Invert the value
      if (value < 280) {     // Compensate for dead zone on potentiometer
        value = 0;
      }
      dispatch(event, &value, sizeof(uint16_t));
    }
    return true;
  }

 private:
  PortBHubTask* portBHub;
  PortBChannel port;
  uint16_t lastSensorValue = 0;
  EventType event;
};
