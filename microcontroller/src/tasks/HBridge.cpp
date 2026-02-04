#include <Arduino.h>
#include <FunctionalInterrupt.h>
#include <M5Tough.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>

#include <tasks/Encoder.cpp>
#include <tasks/I2CHub.cpp>

#include "M5UnitHbridge.h"
#include "events.h"

class HBridgeTask : public Task, public TSEvents::EventHandler {
 public:
  HBridgeTask(Scheduler& s, TSEvents::EventBus& e, I2CHubTask* _i2cHub, EncoderTask* _encoderTask, int _channel, TwoWire& _wire = Wire, int _address = 0x20, unsigned long _interval = 1000 * TASK_MILLISECOND)
      : Task(_interval, TASK_FOREVER, &s, false),
        TSEvents::EventHandler(&s, &e) {
    i2cHub = _i2cHub;
    encoder = _encoderTask;
    channel = _channel;
    wire = &_wire;
    address = _address;
  }

  void HandleEvent(TSEvents::Event event) {
    switch ((EventType)event.id) {
    }
  }

  bool OnEnable() {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return true;
    }
    driver.begin(&Wire, address);
    driver.setDriverDirection(HBRIDGE_FORWARD);
    lastAvg = millis();
    lastAvgCount = 0;
    return true;
  }

  bool Callback() {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      Serial.printf("HBridgeTask unable to set channel %d\n", channel);
      return true;
    }

    if (channel == 0) {  // Stirrer

      rpm = encoder->latestRPM;
      if (rpm < 0 || rpm > 1e5) {
        return true;
      }
      if (rpmSetpoint == 0) {
        driver.setDriverSpeed8Bits(0);
        driverspeed = 0;
        rpmCumError = 0;
      } else {
        rpmSetpoint = constrain(rpmSetpoint, 0, maxRpm);
        rpmError = rpmSetpoint - rpm;
        rpmCumError += rpmError;
        driverspeed = midPWM + round(rpmKp * rpmError + rpmKi * rpmCumError);
        driverspeed = constrain(driverspeed, 0, maxPWM);
        driver.setDriverSpeed8Bits(driverspeed);
      }
      return true;
    }

    if (channel == 1) {  // pump
      driver.setDriverSpeed8Bits(pumppwm);
      return true;
    }

    return true;
  }

  void setRPM(uint16_t _rpm) {
    rpmSetpoint = constrain(_rpm, 0, maxRpm);
  }

  void setPWM(uint16_t _pumppwm) {
    pumppwm = constrain(_pumppwm, 0, maxPWM);
  }

 private:
  uint16_t potvalue = 0;
  uint16_t pumppwm = 0;
  uint16_t stirrerRpm = 0;
  M5UnitHbridge driver;
  int address;
  int lastAvg;
  uint32_t lastAvgCount;
  int channel;
  bool connected = false;
  I2CHubTask* i2cHub;
  EncoderTask* encoder;
  TwoWire* wire;

  // Motor variables
  int driverspeed;
  float rpm = 0.0;

  int maxPWM = 255;
  int midPWM = 127;

  int maxRpm = 120;
  int rpmSetpoint = 100;

  float rpmError = 0.0;
  float rpmCumError = 0.0;

  float rpmKp = 1.4;
  float rpmKi = 0.1;
};