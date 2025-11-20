#include <Arduino.h>
#include <FunctionalInterrupt.h>
#include <M5Tough.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>

#include <tasks/I2CHub.cpp>
#include <tasks/PortBHub.cpp>

#include "UNIT_EXT_ENCODER.h"
#include "events.h"

class FlowSensorTask : public Task, public TSEvents::EventEmitter {
 public:
  FlowSensorTask(Scheduler& s, TSEvents::EventBus& e, I2CHubTask* _i2cHub, int _channel, EventType _event, float _kValue, float _flowCorrectK, TwoWire& _wire = Wire, unsigned long _interval = 1000 * TASK_MILLISECOND)
      : Task(_interval, TASK_FOREVER, &s, false),
        TSEvents::EventEmitter(&e) {
    event = _event;
    i2cHub = _i2cHub;
    channel = _channel;
    wire = &_wire;
    event = _event;
    kValue = _kValue;
    flowCorrectK = _flowCorrectK;
  }

  bool OnEnable() {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return true;
    }
    encoder.begin(&Wire, UNIT_EXT_ENCODER_ADDR, 32, 33, 100000UL);
    encoder.setZeroPulseValue(0);
    lastAvg = millis();
    lastAvgCount = 0;
    return true;
  }

  bool Callback() {
    unsigned long t = millis();
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return true;
    }
    uint32_t count = encoder.getZeroPulseValue();
    float dt = float(t - lastAvg) / 1000;
    float freq = float(count - lastAvgCount) / dt;
    // kValue from data sheet, this should give a value of L/min, Q=f*60/k : e.g k= 1420 for no jet, RS 508-2704 flowmeter
    // kValue & flowCorrect are from the unique .json, flowCorrect is a custom calibration factor to adjust if data sheet k is not accurate
    float flow = (freq * (60 / kValue)) / flowCorrectK;  
    dispatch(event, &flow, sizeof(float));
    lastAvg = t;
    lastAvgCount = count;
    return true;
  }

 private:
  int lastAvg;
  uint32_t lastAvgCount;
  int channel;
  bool connected = false;
  UNIT_EXT_ENCODER encoder;
  I2CHubTask* i2cHub;
  TwoWire* wire;
  EventType event;
  float kValue;
  float flowCorrectK;
};