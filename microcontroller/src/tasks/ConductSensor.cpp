#include <Arduino.h>
#include <M5Tough.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>

#include <tasks/I2CHub.cpp>
#include <tasks/PortBHub.cpp>

#include "events.h"

#include <EEPROM.h>

#include "DFRobot_EC10.h"
// https://wiki.dfrobot.com/Gravity__Analog_Electrical_Conductivity_Sensor___Meter_V2__K%3D1__SKU_DFR0300
// https://wiki.dfrobot.com/Gravity_Analog_Electrical_Conductivity_Sensor_Meter_K=10_SKU_DFR0300-H

class ConductSensorTask : public Task, public TSEvents::EventEmitter {
 public:
  ConductSensorTask(Scheduler& s, TSEvents::EventBus& e, PortBHubTask* _portBHub, PortBChannel _port, EventType _event, unsigned long _interval = 1000 * TASK_MILLISECOND)
      : Task(_interval, TASK_FOREVER, &s, false),
        TSEvents::EventEmitter(&e) {
    portBHub = _portBHub;
    port = _port;
    event = _event;
  }

  bool OnEnable() {
    ec.begin();
    bool ok = portBHub->checkConnection();
    if (!ok) {
      return true;
    }
    return true;
  }

  bool Callback() {
    float ecRaw = portBHub->analogRead(port);
    Wire.beginTransmission(0x70);
    Wire.write(1 << 3);
    Wire.endTransmission();
    lastSensorValue = ecRaw;
    ecVoltage = (ecRaw / 4096.0 * 3300);  // read the voltage - old K=1 sensor
    float ecValue = ec.readEC(ecVoltage, temperature);  // convert voltage to EC with temperature compensation
    dispatch(CONDUCT_SENSOR_DATA, &ecValue, sizeof(float));
    //}
    return true;
  }

  void setTemp(float _temp) {
    temperature = _temp;
  }

 private:
  PortBHubTask* portBHub;
  PortBChannel port;
  float lastSensorValue = 0.0;
  EventType event;
  float ecVoltage, ecValue, temperature = 0.0;
  DFRobot_EC10 ec;
};
