#include <Arduino.h>
#include <FunctionalInterrupt.h>
#include <M5Tough.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>

#include "events.h"

// command receiver for the PPEMD for All - https://ppemd4all.uk/

class SerialRecieverTask : public Task, public TSEvents::EventEmitter {
 public:
  SerialRecieverTask(Scheduler& s, TSEvents::EventBus& e, EventType _event, const char* _deviceName, unsigned long _interval = 1000 * TASK_MILLISECOND)
      : Task(_interval, TASK_FOREVER, &s, false),
        TSEvents::EventEmitter(&e) {
    deviceName = _deviceName;
    event = _event;
  }

  bool OnEnable() {
    serialBuffer.reserve(256);  // Reserve enough space to avoid frequent reallocations
    return true;
  }

  bool Callback() {
    // Read incoming serial data until newline character
    while (Serial.available() > 0) {
      char incomingChar = Serial.read();
      serialBuffer += incomingChar;

      if (incomingChar == '\n') {
        processBuffer(serialBuffer);
        serialBuffer = "";
      }
    }
    return true;
  }

 private:
  EventType event;
  const char* deviceName;
  String serialBuffer;

  void processBuffer(const String& buffer) {
    // Placeholder for processing logic
    // For example, parsing a command or checking for a specific message
    // Emit an event if a certain condition is met
    // if buffer =
    // dispatch(event, &buffer, sizeof(float));
    // Emit(event);

    // message will look like <index>#<value>
    // find the index and value
    int index = buffer.substring(0, buffer.indexOf("#")).toInt();
    float value = buffer.substring(buffer.indexOf("#") + 1).toFloat();

    switch (index) {
      case 0:
        Serial.print("0#");  // these need to be on for the serial PPMD to work!
        Serial.println(deviceName);
        break;
      case 3:
        if (value >= 0 && value <= 65535) {
          uint16_t convertedValue = static_cast<uint16_t>(value);
          dispatch(event, &convertedValue, sizeof(uint16_t));
        }
        break;
      default:
        break;
    }
  }
};