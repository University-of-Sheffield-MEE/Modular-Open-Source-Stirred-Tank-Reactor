A task file looks something like this.

```cpp

#include <Arduino.h>
#define _TASK_OO_CALLBACKS
#define _TASK_STATUS_REQUEST
#include <EventHandler.h>
#include <TaskSchedulerDeclarations.h>
// Add any other imports you need here

#include "events.h"

class BlinkTask : public Task, public TSEvents::EventHandler {
 public:
  BlinkTask(Scheduler &s, TSEvents::EventBus &e, int _pin) // Pin number is being passed to the task from outside to make it generic
      : Task(1000 * TASK_MILLISECOND, TASK_FOREVER, &s, false), // 1000 * TASK_MILLISECOND is how often the loop function (Callback) will run
        TSEvents::EventHandler(&s, &e) {
    pin = _pin;
  }

  // Like Arduino Setup function. One-time preparation things
  bool OnEnable() {
    pinMode(pin, OUTPUT);

    return true; // Returning false would mean 'do not enable', and loop will not be run
  }

  // Like Arduino Loop function. Run at the interval set up at the start
  // Do not use delay etc in here. Do your work and finish as fast as possible. OTherwise it blocks other tasks from running
  bool Callback() {
    // Blink the LED on/off
    digitalWrite(pin, ledState ? HIGH : LOW);
    ledState = !ledState;

    // How you broadcast messages that other tasks can receive. Must be one of the events defined in events.h
    dispatch(LED_BLINK); 

    return true;
  }

  // How you respond to things happening in other tasks
  void HandleEvent(TSEvents::Event event) {
    switch (event.id) {
      case SYSTEM_READY: // Must be one of the events defined in events.h
        disable(); // Stops the task from running its loop function
        break;
    }
  }

  // You can define your own functions like this that allows the task to be controlled from outside
  void SetBlinkRate(int interval) {
    enableIfNot();
    setInterval(interval * TASK_MILLISECOND);
    ledState = false;
  }

 private:
  // Where you define your variables (like the top of the Arduino file)
  int pin;
  bool ledState;
};

```

And you set the task going like this in main.cpp:

```cpp
// ...other includes
#include "tasks/BlinkTask.cpp"

// ...other task initialisers
BlinkTask blinkTask(ts, e, LED_BUILTIN);

void setup() {
  M5.begin();
  Serial.begin(115200);

  // ...other enables
  blinkTask.enable();
}

void loop() {
  ts.execute();
  yield();
}
```

