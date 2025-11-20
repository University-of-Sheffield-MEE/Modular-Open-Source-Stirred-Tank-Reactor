#pragma once

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

class EncoderTask : public Task, public TSEvents::EventEmitter {
 public:
  EncoderTask(Scheduler& s, TSEvents::EventBus& e, I2CHubTask* _i2cHub, int _channel, EventType _event, TwoWire& _wire = Wire, unsigned long _interval = 1000 * TASK_MILLISECOND)
      : Task(_interval, TASK_FOREVER, &s, false),
        TSEvents::EventEmitter(&e) {
    i2cHub = _i2cHub;
    channel = _channel;
    wire = &_wire;
    event = _event;

    numReadings = 5;
    rpmReadings = new double[numReadings];
    for (int i = 0; i < numReadings; i++) {
      rpmReadings[i] = 0;
    }
  }

  bool OnEnable() {
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return true;
    }
    encoder.begin(&Wire, UNIT_EXT_ENCODER_ADDR, 32, 33, 100000UL);
    encoder.setZeroPulseValue(0);
    lastAvg = millis();
    lastAvgCount = 0.00000;
    return true;
  }

  bool Callback() {
    String encodername = "";
    double t = millis();
    bool ok = i2cHub->setChannel(channel);
    if (!ok) {
      return true;
    }
    uint32_t rawcount = encoder.getEncoderValue();
    double count = rawcount;
    double dt = double(t - lastAvg);
    double rpm = (double(count - lastAvgCount) / 1400.00) / dt;
    rpm = rpm * 60000.00;
    updateRollingAverage(rpm);
    dispatch(event, &averageRPM, sizeof(double));
    lastAvg = t;
    lastAvgCount = count;
    latestRPM = averageRPM;
    return true;
  }

  double latestRPM;

 private:
  double lastAvg;
  double lastAvgCount;
  int channel;
  bool connected = false;
  UNIT_EXT_ENCODER encoder;
  I2CHubTask* i2cHub;
  TwoWire* wire;
  EventType event;

  int numReadings;      // Number of readings to average
  double* rpmReadings;  // Array to store RPM readings for rolling average
  double averageRPM;    // Running average of RPM

  void updateRollingAverage(double newRPM) {
    // Shift existing readings
    for (int i = numReadings - 1; i > 0; i--) {
      rpmReadings[i] = rpmReadings[i - 1];
    }
    // Add new reading
    rpmReadings[0] = newRPM;

    // Calculate running total
    double total = 0;
    for (int i = 0; i < numReadings; i++) {
      total += rpmReadings[i];
    }

    // Calculate running average
    averageRPM = total / numReadings;
  }
};