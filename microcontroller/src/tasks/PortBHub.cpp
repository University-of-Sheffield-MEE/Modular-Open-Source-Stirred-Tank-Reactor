#pragma once
#include <Arduino.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <M5_KMeter.h>
#include <TaskSchedulerDeclarations.h>

#include <tasks/I2CHub.cpp>

#include "events.h"

// For pin addresses and command codes, see table at bottom of:
// https://docs.m5stack.com/en/unit/pbhub

enum PortBChannel : uint8_t {
  PORTB_CH0 = 0x40,
  PORTB_CH1 = 0x50,
  PORTB_CH2 = 0x60,
  PORTB_CH3 = 0x70,
  PORTB_CH4 = 0x80,
  PORTB_CH5 = 0xA0,
};

enum PortBPin : uint8_t {
  PORTB_PIN0 = 0x00,
  PORTB_PIN1 = 0x01,
};

class PortBHubTask : public Task, public TSEvents::EventEmitter {
 public:
  PortBHubTask(Scheduler& s, TSEvents::EventBus& e, I2CHubTask* _i2cHub, int _channel, int _address = 0x61, TwoWire& _wire = Wire)
      : Task(TASK_IMMEDIATE, TASK_ONCE, &s, false),
        TSEvents::EventEmitter(&e) {
    i2cHub = _i2cHub;
    channel = _channel;
    address = _address;
    wire = &_wire;
  }

  bool OnEnable() {
    return true;
  }

  bool Callback() {
    checkConnection();
    return true;
  }

  bool checkConnection() {
    i2cHub->setChannel(channel);
    wire->beginTransmission(address);
    int result = wire->endTransmission();

    dispatch(result == 0 ? PORTB_HUB_CONNECTED : PORTB_HUB_ERROR);

    return result == 0;
  }

  uint16_t analogRead(PortBChannel port) {
    bool ok = sendCommand(port, 0x06);
    if (!ok) {
      return 0;
    }

    uint8_t valueLowByte = 0;
    uint8_t valueHighByte = 0;

    wire->requestFrom(address, (uint8_t)2);
    while (wire->available()) {
      valueLowByte = wire->read();
      valueHighByte = wire->read();
    }

    return (valueHighByte << 8) | valueLowByte;
  }

  uint8_t digitalRead(PortBChannel port, PortBPin pin) {
    bool ok = sendCommand(port, 0x04 | pin);
    if (!ok) {
      return 0;
    }

    uint8_t value = 0;

    wire->requestFrom(address, (uint8_t)1);
    while (wire->available()) {
      value = wire->read();
    }
    return value;
  }

  bool digitalWrite(PortBChannel port, PortBPin pin, uint8_t value) {
    return sendCommand(port, 0x00 | pin, &value);
  }

  bool analogWrite(PortBChannel port, PortBPin pin, uint8_t value) {
    return sendCommand(port, 0x02 | pin, &value);
  }

 private:
  bool sendCommand(PortBChannel port, uint8_t command, uint8_t* value = NULL) {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return false;
    }
    wire->beginTransmission(address);
    wire->write(port | command);
    if (value != NULL) {
      wire->write(*value);
    }
    int result = wire->endTransmission();
    if (result != 0) {
      dispatch(PORTB_HUB_ERROR);
    }
    return result == 0;
  }

  I2CHubTask* i2cHub;
  int channel;
  int address;
  TwoWire* wire;
};