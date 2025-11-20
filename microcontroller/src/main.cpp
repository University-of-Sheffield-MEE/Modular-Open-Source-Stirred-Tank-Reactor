#include <Arduino.h>
#include <M5Tough.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EEPROM.h>
#include <EventBus.h>
#include <M5_KMeter.h>
#include <TaskScheduler.h>
#include <TaskSchedulerDeclarations.h>
#include <Wire.h>

#include "DFRobot_EC10.h"
#include "config.h"
#include "events.h"
#include "tasks/AngleSensor.cpp"
#include "tasks/ConductSensor.cpp"
#include "tasks/Eduroam.cpp"
#include "tasks/Encoder.cpp"
#include "tasks/FlowSensor.cpp"
#include "tasks/HBridge.cpp"
#include "tasks/HomeAssistant.cpp"
#include "tasks/I2CHub.cpp"
#include "tasks/MQTT.cpp"
#include "tasks/PortBHub.cpp"
#include "tasks/Renderer.cpp"
#include "tasks/SerialReciever.cpp"
#include "tasks/Thermocouple.cpp"

float ecVoltage, ecValue, temperature = 25;
DFRobot_EC10 ec;

// https://wiki.dfrobot.com/Gravity_Analog_Electrical_Conductivity_Sensor_Meter_K=10_SKU_DFR0300-H

Scheduler ts;
TSEvents::EventBus e;

hassSensor sensors[4] = {
    {"cond_rate", "Water Conductivity", "temperature", "ms/cm", CONDUCT_SENSOR_DATA, 0, false},
    {"water_temp", "Water Temperature", "temperature", "°C", THERMOCOUPLE_DATA, 0, false},
    {"flow_rate", "Flow Rate", "water", "l/min", FLOW_SENSOR_1_DATA, 0, false},
    //{"stirrer_rate", "Rotation Rate", "water", "rpm", ENCODER_1_DATA, 0, false}, //rpm
    {"flow_rate2", "Flow Rate", "water", "l/min", ENCODER_1_DATA, 0, false},
};

MQTTTask* mqttTask;
HomeAssistantTask* homeAssistantTask;
EduroamTask* wifiTask;

RendererTask* renderer;
I2CHubTask* i2cHubTask;
EncoderTask* encoderTask1;        // Stirrer
EncoderTask* encoderTask2;        // Pump - Not used
HBridgeTask* HBridgeOutputTask1;  // Stirrer
HBridgeTask* HBridgeOutputTask2;  // Pump
PortBHubTask* portBHubTask;
AngleSensorTask* angleSensor1;         // Stirrer
AngleSensorTask* angleSensor2;         // Pump
ConductSensorTask* conductSensorTask;  // Conductivity Sensor
FlowSensorTask* flowSensor1Task;       // Inflow
ThermocoupleTask* waterTempTask;
SerialRecieverTask* serialRecieverTask;

bool hasRunOnce = false;
int initialDecision = 0;
bool isISOThermocoupleSetup = false;  // Global variable to store thermocouple type - ISO or not for initial screen

class EventBridge : TSEvents::EventHandler {
 public:
  EventBridge(Scheduler& s, TSEvents::EventBus& e) : TSEvents::EventHandler(&s, &e) {}

  void HandleEvent(TSEvents::Event e) {
    switch (e.id) {
      case SERIAL_DATA: {
        HBridgeOutputTask1->setRPM(*(uint16_t*)e.data);
        break;
      }
      case ANGLE_SENSOR_1_DATA: {
        uint16_t rpm = *(uint16_t*)e.data;
        rpm = rpm * 150 / 4096;
        HBridgeOutputTask1->setRPM(rpm);
        break;
      }
      case ANGLE_SENSOR_2_DATA: {
        uint16_t _pumppwm = *(uint16_t*)e.data;
        _pumppwm = _pumppwm * 255 / 4096;
        HBridgeOutputTask2->setPWM(_pumppwm);
        break;
      }
      case THERMOCOUPLE_DATA: {
        float _temp = *(float*)e.data;
        conductSensorTask->setTemp(_temp);
        break;
      }
    }
  }
};

EventBridge eventBridge(ts, e);

void renderConfigError(const char* e) {
  M5.Lcd.print(e);
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  EEPROM.begin(255);
  ec.begin();
  Serial.begin(115200);
  const char* configError = loadConfigFile();
  if (configError != NULL) {
    Serial.println(configError);
    return;
  }
  renderer = new RendererTask(ts, e, getConfigValue("deviceId"), VERSION);

  //----------------------------------------------------
  // Setup In relation to network connections:
  //----------------------------------------------------

  wifiTask = new EduroamTask(ts, e, getConfigValue("wifiUser"), getConfigValue("wifiPass"));
  mqttTask = new MQTTTask(ts, e, getConfigValue("mqttServer"), getConfigIntValue("mqttPort"), getConfigValue("deviceId"));
  homeAssistantTask = new HomeAssistantTask(ts, e, mqttTask, getConfigValue("deviceId"), sensors, 4, 2 * TASK_SECOND);  // this needs to optimised for not causing a data bottle neck

  //----------------------------------------------------
  // Setup In relation to physical connections:
  //----------------------------------------------------

  // I2C Output to PaHub I2C Multiplexer
  Wire.begin(32, 33);  // declaration from the M5stack I2C pins...I think
  serialRecieverTask = new SerialRecieverTask(ts, e, SERIAL_DATA, getConfigValue("deviceId"), 500 * TASK_MILLISECOND);
  i2cHubTask = new I2CHubTask(ts, e, 0x70, Wire);
  // PaHub Connection 0 - 3 way Splitter 1 (stirrer)
  // Splitter 1 - Connection 2 - Ext-Encoder (Stirrer)
  encoderTask1 = new EncoderTask(ts, e, i2cHubTask, 0, ENCODER_1_DATA, Wire, 100 * TASK_MILLISECOND);  // for encoder
  // Splitter 1 - Connection 1 - Hbridge (Stirrer)
  HBridgeOutputTask1 = new HBridgeTask(ts, e, i2cHubTask, encoderTask1, 0, Wire, 0x20, 100 * TASK_MILLISECOND);
  // Splitter 1 - Connection 3 - EMPTY

  // PaHub Connection 1 - Hbridge (Pump)
  HBridgeOutputTask2 = new HBridgeTask(ts, e, i2cHubTask, encoderTask2, 1, Wire, 0x20, 100 * TASK_MILLISECOND);

  // PaHub Connection 2 - PbHub IN
  portBHubTask = new PortBHubTask(ts, e, i2cHubTask, 2, 0x61, Wire);
  // PbHub Connection 0 - Angle Sensor 1
  angleSensor1 = new AngleSensorTask(ts, e, portBHubTask, PORTB_CH0, ANGLE_SENSOR_1_DATA, 100 * TASK_MILLISECOND);
  // PbHub Connection 1 - Angle Sensor 2
  angleSensor2 = new AngleSensorTask(ts, e, portBHubTask, PORTB_CH1, ANGLE_SENSOR_2_DATA, 100 * TASK_MILLISECOND);
  // PbHub Connection 2
  conductSensorTask = new ConductSensorTask(ts, e, portBHubTask, PORTB_CH2, CONDUCT_SENSOR_DATA, 100 * TASK_MILLISECOND);
  // PbHub Connection 3-5 EMPTY

  // PaHub Connection 3 - Thermocouple
  waterTempTask = new ThermocoupleTask(ts, e, i2cHubTask, 3, 0x66, Wire, 1000 * TASK_MILLISECOND);  // for thermocouple

  // PaHub Connection 4 - Ext-Encoder (Flowmeter 1) - INFLOW
  flowSensor1Task = new FlowSensorTask(ts, e, i2cHubTask, 5, FLOW_SENSOR_1_DATA, getConfigFloatValue("flowK", 1.0), getConfigFloatValue("flowCorrectK", 1.0), Wire, 500 * TASK_MILLISECOND);

  // PaHub Connection 5 - Not used

  //----------------------------------------------------
  // Task enabling setup
  //----------------------------------------------------

  renderer->enable();
  wifiTask->enable();
  serialRecieverTask->enable();
  mqttTask->enable();
  homeAssistantTask->enable();
  i2cHubTask->enable();
  encoderTask1->enable();        // Stirrer
  HBridgeOutputTask1->enable();  // Stirrer
  HBridgeOutputTask2->enable();  // Pump
  portBHubTask->enable();
  angleSensor1->enable();  // Stirrer
  angleSensor2->enable();  // Pump

  flowSensor1Task->enable();  // Inflow
  waterTempTask->enable();    // Thermocouple

  static const uint16_t COL_BG = TFT_BLACK;  // originally TFT_WHITE
  static const uint16_t COL_FG = TFT_WHITE;  // 0xF81F;
  M5.Lcd.begin();

  //----------------------------------------------------
  // Initial Decision Screen
  //----------------------------------------------------

  M5.Lcd.setFont(&FreeSans18pt7b);
  M5.Lcd.fillRect(0, 0, 320, 240, COL_FG);
  // Calibrate Button
  Button calBtn(0, 40, 160, 200);
  M5.Lcd.fillRect(0, 40, 160, 200, BLUE);
  M5.Lcd.setCursor(40, 150);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.println("YES");
  // Run Button
  Button runBtn(160, 40, 160, 200);
  M5.Lcd.fillRect(160, 40, 160, 200, YELLOW);
  M5.Lcd.setCursor(220, 150);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.println("NO");
  // Text
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(30, 30);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.println("Calibrate Probe?");

  while (initialDecision == 0) {
    M5.update();
    if (calBtn.wasPressed()) {
      initialDecision = 1;
      break;
    }
    if (runBtn.wasPressed()) {
      initialDecision = 2;
      break;
    }
    delay(500);
  }
  int calibrationDecision = 0;
  //----------------------------------------------------
  // Calibration Screen
  //----------------------------------------------------

  if (initialDecision == 1) {
    M5.update();
    M5.Lcd.fillRect(0, 0, 320, 240, COL_FG);

    // Header Section
    M5.Lcd.fillRect(0, 0, 320, 25, YELLOW);
    M5.Lcd.setCursor(90, 18);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFont(&FreeSansBold9pt7b);
    M5.Lcd.println("Calibration Setup:");

    // Live feed to the water temperature
    M5.Lcd.fillRect(238, 38, 50, 40, COL_BG);
    M5.Lcd.setCursor(30, 54);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFont(&FreeSans9pt7b);
    M5.Lcd.println("T/C Temp (°C): ");

    // Feedback from serial
    M5.Lcd.fillRect(0, 160, 320, 100, LIGHTGREY);
    M5.Lcd.setCursor(5, 183);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFont(&FreeSans9pt7b);
    M5.Lcd.println("Wash the probe with d/w, dry, Insert in 12.88 us/cm buffer,                  Press Enterec");

    // Live feed to the water conductivity
    M5.Lcd.setCursor(30, 74);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.println("Conductivity (ms/cm): ");

    // Calibration Buttons
    Button EnterecBtn(2, 94, 105, 50);
    M5.Lcd.fillRect(2, 94, 105, 50, BLUE);
    M5.Lcd.setFont(&FreeSansBold9pt7b);
    M5.Lcd.setCursor(20, 125);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Enterec");

    Button CalecBtn(108, 94, 105, 50);
    M5.Lcd.fillRect(108, 94, 105, 50, GREEN);
    M5.Lcd.setCursor(136, 125);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Calec");

    Button ExitecBtn(214, 94, 104, 50);
    M5.Lcd.fillRect(214, 94, 104, 50, ORANGE);
    M5.Lcd.setCursor(242, 125);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Exitec");

    // Finish Button
    Button ExitBtn(295, 0, 25, 25);
    M5.Lcd.fillRect(295, 0, 25, 25, RED);
    M5.Lcd.setCursor(303, 16);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("X");

    //  Determine Thermocouple Type at the beginning of the calibration screen
    i2cHubTask->setChannel(3);
    Wire.beginTransmission(0x66);
    Wire.write(0xfe);
    Wire.endTransmission(false);
    Wire.requestFrom(0x66, 1);
    if (Wire.available()) {
      byte version = Wire.read();
      isISOThermocoupleSetup = version > 0;
    } else {
      Serial.println("Error reading thermocouple version during setup.");
    }

    // Initialize M5_KMeter sensor here, before the calibration loop
    M5_KMeter sensor;
    sensor.begin(&Wire, 0x66);

    for (int i = 0; i < 8; i++) {
      EEPROM.write(0x0F + i, 0xFF);  // write defaullt value to the EEPROM
      delay(10);
    }
    ec.begin();

    while (calibrationDecision != 4) {
      // Thermocouple coms
      M5.update();
      i2cHubTask->setChannel(3);  // Ensure correct I2C channel is selected
      delay(10);                  // Small delay after channel selection
      Wire.beginTransmission(0x66);
      Wire.write(0);
      Wire.endTransmission(false);
      Wire.requestFrom(0x66, 2);
      byte byte0 = Wire.read();
      byte byte1 = Wire.read();
      float temperatureRead;
      if (isISOThermocoupleSetup) {
        temperatureRead = (byte1 << 8) | byte0;
        temperatureRead /= 100.0;
      } else {
        temperatureRead = (byte0 << 8) | byte1;
        temperatureRead /= 16.0;
      }
      M5.Lcd.fillRect(238, 40, 50, 18, COL_BG);
      M5.Lcd.setCursor(240, 54);
      M5.Lcd.setTextColor(RED);
      M5.Lcd.println(temperatureRead, 1);
      temperature = temperatureRead;  // Update the global temperature variable

      // Conductivity Sensor coms
      static unsigned long timepoint = millis();
      if (millis() - timepoint > 1000U) {  // time interval: 1s
        float ecRaw = portBHubTask->analogRead(PORTB_CH2);
        timepoint = millis();
        ecVoltage = (ecRaw / 4096.0 * 3300);  // read the voltage
        float ecValue = ec.readEC(ecVoltage, temperatureRead);  // convert voltage to EC with temperature compensation
        M5.Lcd.fillRect(238, 58, 50, 18, COL_BG);
        M5.Lcd.setCursor(240, 74);
        M5.Lcd.setTextColor(RED);
        M5.Lcd.println(ecValue, 2);
      }
      delay(100);

      if (EnterecBtn.wasPressed()) {
        calibrationDecision = 1;
        char cmd[20];                                 // Define the cmd variable, Assuming a maximum length of 20 characters
        strcpy(cmd, "ENTEREC");                       // Set the content of cmd to "ENTEREC"
        ec.calibration(ecVoltage, temperature, cmd);  // Call the calibration function and pass a pointer to cmd
      }
      if (CalecBtn.wasPressed()) {
        calibrationDecision = 2;
        char cmd[20];                                 // Define the cmd variable, Assuming a maximum length of 20 characters
        strcpy(cmd, "CALEC");                         // Set the content of cmd to "CALEC"
        ec.calibration(ecVoltage, temperature, cmd);  // Call the calibration function and pass a pointer to cmd
      }
      if (ExitecBtn.wasPressed()) {
        calibrationDecision = 3;
        char cmd[20];                                 // Define the cmd variable, Assuming a maximum length of 20 characters
        strcpy(cmd, "EXITEC");                        // Set the content of cmd to "CALEC"
        ec.calibration(ecVoltage, temperature, cmd);  // Call the calibration function and pass a pointer to cmd
      }
      if (ExitBtn.wasPressed()) {
        calibrationDecision = 4;
        break;
      }
    }
    M5.Lcd.setTextSize(1);
    renderer->initialRender();
  }

  conductSensorTask->enable();  // Conductivity Sensor
  M5.Lcd.setTextSize(1);
  renderer->initialRender();
  M5.update();
}

//----------------------------------------------------
// Main Loop
//----------------------------------------------------

void loop() {
  ts.execute();
  yield();
}