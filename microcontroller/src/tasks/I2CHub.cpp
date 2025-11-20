#pragma once

#include <Arduino.h>
#include <Wire.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>
#include "events.h"
#include "version.h"

class I2CHubTask : public Task, public TSEvents::EventEmitter {
 public:
  I2CHubTask(Scheduler &s, TSEvents::EventBus &e, int _address = 0x70, TwoWire &w = Wire)
      : Task(TASK_IMMEDIATE, TASK_ONCE, &s, false),
        TSEvents::EventEmitter(&e) {
    address = _address;
    wire = &w;
  }

  bool OnEnable() {
    return true;
  }

  bool Callback() {
    checkConnection();
    return true;
  }

  bool checkConnection() {
    wire->beginTransmission(address);
    int result = wire->endTransmission();

    dispatch(result == 0 ? I2C_HUB_CONNECTED : I2C_HUB_ERROR);

    return result == 0;
  }

  bool setChannel(int channel) {
    if (channel < 0 || channel > 7) {
      return false;
    }

    wire->beginTransmission(address);
    wire->write(1 << channel);
    int result = wire->endTransmission();

    if (result != 0) {
      dispatch(I2C_HUB_ERROR);
    }

    return result == 0;
  }

  void scan() {
    byte error;
    int nDevices;

    Serial.println("I2CHub-Scan: Scanning...");
    nDevices = 0;
    for (byte address = 1; address < 127; address++) {
      wire->beginTransmission(address);
      error = wire->endTransmission();

      if (error == 0) {
        Serial.print("I2CHub-Scan: I2C device found at address 0x");
        if (address < 16) {
          Serial.print("0");
        }
        Serial.println(address, HEX);

        nDevices++;
      } else if (error == 4) {
        Serial.print("I2CHub-Scan: Unknown error at address 0x");
        if (address < 16) {
          Serial.print("0");
        }
        Serial.println(address, HEX);
      }
    }
    if (nDevices == 0) {
      Serial.println("I2CHub-Scan: No I2C devices found\n");
    } else {
      Serial.println("I2CHub-Scan: done\n");
    }
  }

 private:
  TwoWire *wire;
  int address;
  const char *url;
};