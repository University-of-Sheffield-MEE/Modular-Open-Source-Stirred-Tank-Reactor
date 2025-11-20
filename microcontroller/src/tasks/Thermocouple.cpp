#include <Arduino.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <M5_KMeter.h>
#include <TaskSchedulerDeclarations.h>

#include <tasks/I2CHub.cpp>

#include "events.h"

class ThermocoupleTask : public Task, public TSEvents::EventEmitter {
 public:
  ThermocoupleTask(Scheduler& s, TSEvents::EventBus& e, I2CHubTask* _i2cHub, int _channel, int _address = 0x60, TwoWire& _wire = Wire, unsigned long _interval = 1000 * TASK_MILLISECOND)
      : Task(_interval, TASK_FOREVER, &s, false),
        TSEvents::EventEmitter(&e) {
    i2cHub = _i2cHub;
    channel = _channel;
    address = _address;
    wire = &_wire;
  }

  bool OnEnable() {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return true;
    }

    getIsISO(&isISO);
    return true;
  }

  bool Callback() {
    float temperature;
    bool ok = getSensorTemperature(&temperature);
    if (ok) {
      dispatch(THERMOCOUPLE_DATA, &temperature, sizeof(float));
    }
    return true;
  }

  bool getIsISO(bool* out) {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return false;
    }

    wire->beginTransmission(address);
    wire->write(0xfe);
    wire->endTransmission(false);
    wire->requestFrom(address, 1);
    byte version = wire->read();

    bool isISO = version > 0;
    *out = isISO;

    return true;
  }

  bool getSensorTemperature(float* out = NULL) {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return false;
    }
    wire->beginTransmission(address);
    wire->write(0);
    wire->endTransmission(false);
    wire->requestFrom(address, 2);
    byte byte0 = wire->read();
    byte byte1 = wire->read();
    float temp;
    if (isISO) {
      temp = (byte1 << 8) | byte0;
      temp /= 100.0;
    } else {
      temp = (byte0 << 8) | byte1;
      temp /= 16.0;
    }
    if (!connected) {
      dispatch(THERMOCOUPLE_CONNECTED);
      connected = true;
    }
    if (out) {
      *out = temp;
    }

    return true;
  }

 private:
  int address;
  int channel;
  bool connected = false;
  bool isISO = false;  // Is the sesnsor the vanilla thermcouple or the ISO one?
  I2CHubTask* i2cHub;
  TwoWire* wire;
};