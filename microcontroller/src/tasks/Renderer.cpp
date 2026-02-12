#include <Arduino.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EEPROM.h>
#include <EventHandler.h>
#include <M5Tough.h>
#include <TaskSchedulerDeclarations.h>
#include <assets/Regular 400.h>
#include <events.h>

enum class IndicatorState : unsigned char {
  OFF = 0,
  WARN,
  ERROR,
  OK
};

enum class IndicatorType : unsigned char {
  I2C,
  TEMP,
};

typedef struct RenderState {
  float FlowSensor1Data;
  double Encoder1Data;
  uint16_t AngleSensor1Data;
  uint16_t AngleSensor2Data;
  float waterTemp;
  float conductTemp;
  IndicatorState indicators[6];

  void setIndicator(IndicatorType type, IndicatorState state) {
    indicators[(int)type] = state;
  }
} RenderState;

static const RenderState defaultRenderState = {};

class RendererTask : public Task, public TSEvents::EventHandler {
 public:
  RendererTask(Scheduler& s, TSEvents::EventBus& e, const char* _deviceId, const char* _version)
      : Task(100 * TASK_MILLISECOND, TASK_FOREVER, &s, false),
        TSEvents::EventHandler(&s, &e) {
    lastRenderState = defaultRenderState;
  }

  bool OnEnable() {
    M5.Lcd.begin();
    initialRender();
    return true;
  }

  bool Callback() {
    M5.update();
    return true;
  }

  void HandleEvent(TSEvents::Event event) {
    RenderState renderState = lastRenderState;
    switch ((EventType)event.id) {
      case SERIAL_DATA:
        renderState.AngleSensor1Data = *(uint16_t*)event.data;
        render(renderState);
        break;

      case ANGLE_SENSOR_1_DATA:
        renderState.AngleSensor1Data = *(uint16_t*)event.data * 380 / 4096; // x / 4096 is the rpm limit
        render(renderState);
        break;
      case ANGLE_SENSOR_2_DATA:
        renderState.AngleSensor2Data = *(uint16_t*)event.data;
        render(renderState);
        break;
      case FLOW_SENSOR_1_DATA:  // Inflow
        renderState.FlowSensor1Data = *(float*)event.data;
        render(renderState);
        break;
      case THERMOCOUPLE_DATA:
        renderState.waterTemp = *(float*)event.data;
        render(renderState);
        break;
      case CONDUCT_SENSOR_DATA:
        renderState.conductTemp = *(float*)event.data;
        render(renderState);
        break;
      case ENCODER_1_DATA:  // Stirrer
        renderState.Encoder1Data = *(double*)event.data;
        render(renderState);
        break;
      case I2C_HUB_CONNECTED:
        renderState.setIndicator(IndicatorType::I2C, IndicatorState::OK);
        render(renderState);
        break;
      case I2C_HUB_ERROR:
        renderState.setIndicator(IndicatorType::I2C, IndicatorState::ERROR);
        render(renderState);
        break;
    }
  }

  void initialRender() {  // screen size is 320 x 240 pixels
    M5.update();
    M5.Lcd.setTextSize(1);
    // Static Text
    M5.Lcd.setFreeFont(&FreeSans8pt7b);
    M5.Lcd.setTextColor(COL_BG, COL_FG);
    M5.Lcd.setCursor(0, 10);
    M5.Lcd.println("Initial Render OK");
    M5.Lcd.fillRect(0, 0, 320, 240, COL_FG);

    // Header Section
    M5.Lcd.fillRect(0, 0, 320, 25, YELLOW);  // clear screen
    M5.Lcd.setCursor(45, 18);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.setFont(&FreeSansBold9pt7b);
    M5.Lcd.println("Stirred Tank Experiment:");

    // Data Spaces initial render
    // Backgrounds
    M5.Lcd.fillRect(0, 48, 320, 25, 0xf6da);
    M5.Lcd.fillRect(220, 48, 70, 25, COL_BG);  // Temperature

    M5.Lcd.fillRect(0, 77, 320, 25, 0xa6ff);
    M5.Lcd.fillRect(220, 77, 70, 25, COL_BG);  // Flowrate

    M5.Lcd.fillRect(0, 106, 320, 25, 0xa6ff);
    M5.Lcd.fillRect(220, 106, 70, 25, COL_BG);  // Pump Power

    M5.Lcd.fillRect(0, 135, 320, 25, 0xcfb9);
    M5.Lcd.fillRect(220, 135, 70, 25, COL_BG);  // Stirrer Set Point

    M5.Lcd.fillRect(0, 164, 320, 25, 0xcfb9);
    M5.Lcd.fillRect(220, 164, 70, 25, COL_BG);  // Stirrer Actual

    M5.Lcd.fillRect(0, 193, 320, 25, 0xd67e);
    M5.Lcd.fillRect(220, 193, 70, 25, COL_BG);  // Conductivity Sensor
    // Fixed Text
    M5.Lcd.setCursor(30, 66);
    M5.Lcd.setTextColor(BLACK);
    M5.Lcd.println("T/C Temp (°C): ");
    M5.Lcd.setCursor(30, 95);
    M5.Lcd.println("Tap Inflow (L/min): ");
    M5.Lcd.setCursor(30, 124);
    M5.Lcd.println("Pump/Out Pwr (%): ");
    M5.Lcd.setCursor(30, 153);
    M5.Lcd.println("Stirrer S/P (rpm): ");
    M5.Lcd.setCursor(30, 182);
    M5.Lcd.println("Stirrer Act (rpm): ");
    M5.Lcd.setCursor(30, 211);
    M5.Lcd.println("Cond-Sens (ms/cm): ");

    // Dynamic Text
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(232, 66);  // Temperature
    M5.Lcd.println("00.00");
    M5.Lcd.setCursor(232, 95);  // Flowrate
    M5.Lcd.println("00.00");
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(232, 124);  // Pump Power
    M5.Lcd.println("00.00");
    M5.Lcd.setCursor(232, 153);  // Stirrer Set Point
    M5.Lcd.println("00.00");
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(232, 182);  // Stirrer Actual
    M5.Lcd.println("00.00");
    M5.Lcd.setCursor(232, 211);  // Conductivity Sensor
    M5.Lcd.println("00.00");
  }

  void render(RenderState newState) {
    if (newState.AngleSensor1Data != lastRenderState.AngleSensor1Data) {  // Stirrer rpm
      renderAngleSensor1Temp(newState.AngleSensor1Data);
    }
    if (newState.AngleSensor2Data != lastRenderState.AngleSensor2Data) {  // Input Flowrate Set Point
      renderAngleSensor2Temp(newState.AngleSensor2Data);
    }
    if (newState.FlowSensor1Data != lastRenderState.FlowSensor1Data) {  // Inflow
      renderFlowRate1(newState.FlowSensor1Data);
    }
    if (newState.waterTemp != lastRenderState.waterTemp) {  // Thermocouple
      renderWaterTemp(newState.waterTemp);
    }
    if (newState.conductTemp != lastRenderState.conductTemp) {  // Conductivity Sensor
      renderConductTemp(newState.conductTemp);
    }
    if (newState.Encoder1Data != lastRenderState.Encoder1Data) {  // Stirrer
      renderEncoder1Data(newState.Encoder1Data);
    }
    if (newState.waterTemp != lastRenderState.waterTemp) {  // Thermocouple
      renderWaterTemp(newState.waterTemp);
    }
    lastRenderState = newState;
  }

 private:
  static const uint16_t COL_BG = TFT_BLACK;
  static const uint16_t COL_FG = TFT_WHITE;

  // Thermocouple loop render
  void renderWaterTemp(float waterTemp) {
    M5.Lcd.fillRect(220, 48, 70, 25, COL_BG);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(232, 66);  // Temperature
    M5.Lcd.println(waterTemp, 2);
    Serial.print("1#");  // hook for the PPEMD for All - https://ppemd4all.uk/
    Serial.println(waterTemp);
  }

  // Flow Sensor 1 loop render - Inflow
  void renderFlowRate1(float FlowSensor1Data) {
    M5.Lcd.fillRect(220, 77, 70, 25, COL_BG);
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(232, 95);  // Flowrate
    M5.Lcd.println(FlowSensor1Data);
    Serial.print("2#");  // hook for the PPEMD for All - https://ppemd4all.uk/
    Serial.println(FlowSensor1Data);
  }

  // Angle Sensor 2 loop render - Input Pump Power Set Point
  void renderAngleSensor2Temp(float AngleSensor2Data) {
    M5.Lcd.fillRect(220, 106, 70, 25, COL_BG);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(232, 124);  // Pump Power
    AngleSensor2Data = (AngleSensor2Data / 40.96);
    M5.Lcd.println(int(AngleSensor2Data));
  }

  // Angle Sensor 1 loop render - Stirrer set point rpm
  void renderAngleSensor1Temp(float AngleSensor1Data) {
    M5.Lcd.fillRect(220, 135, 70, 25, COL_BG);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setCursor(232, 153);  // Stirrer Set Point
    M5.Lcd.println(int(AngleSensor1Data));
  }

  // Encoder 1 loop render - Stirrer
  void renderEncoder1Data(double Encoder1Data) {
    M5.Lcd.fillRect(220, 164, 70, 25, COL_BG);  // Stirrer Actual
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(232, 182);  // Stirrer Actual
    Serial.print("3#");  // hook for the PPEMD for All - https://ppemd4all.uk/
    Serial.println(int(Encoder1Data));
    M5.Lcd.println(int(Encoder1Data));
  }

  // Conducitvity Sensor loop render
  void renderConductTemp(float conductTemp) {
    M5.Lcd.fillRect(220, 193, 70, 25, COL_BG);  // Conductivity Sensor
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(232, 211);  // Conductivity Sensor
    M5.Lcd.println(conductTemp, 2);
    Serial.print("4#");  // hook for the PPEMD for All - https://ppemd4all.uk/
    Serial.println(conductTemp, 2);
  }

  void printWithSpacing(const char* str, float spacing) {
    int len = strlen(str);
    int startX = M5.Lcd.getCursorX();
    for (int i = 0; i < len; i++) {
      M5.Lcd.print(str[i]);
      int newX = M5.Lcd.getCursorX();
      if (i < len - 1) {
        int scaledX = startX + (newX - startX) * spacing;
        M5.Lcd.setCursor(scaledX, M5.Lcd.getCursorY());
      }
    }
  }

  void printWithSpacing(double value, int decimalPlaces, float spacing) {
    char buffer[16];
    dtostrf(value, 0, decimalPlaces, buffer);
    printWithSpacing(buffer, spacing);
  }
  RenderState lastRenderState;
};