#include <Arduino.h>
#include "conf.h"

#if LIB_SEL == USE_STEPPER_LIB
// Install the Stepper library provided by Arduino first
#include <Stepper.h>
#else
#include <AccelStepper.h>
#endif

#if LIB_SEL == USE_STEPPER_LIB
// Stepper object for the BYJ-48 motor device
Stepper MY_STEPPER = Stepper(
  DEFAULT_STEPS_PER_REV, 
  PIN_IN1, 
  PIN_IN3, 
  PIN_IN2, 
  PIN_IN4
);
#else
AccelStepper MY_STEPPER = AccelStepper(
  MOTOR_INTERFACE_TYPE,
  PIN_IN1,
  PIN_IN3,
  PIN_IN2,
  PIN_IN4
);
#endif

void loopUsingStepperLib();
void setupUsingStepperLib();

void loopUsingAccelStepperLib();
void setupUsingAccelStepperLib();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Stepper example code for the BYJ-48 device");
  pinMode(LED_BUILTIN, OUTPUT);
  
#if LIB_SEL == USE_STEPPER_LIB
  setupUsingStepperLib();
#else
  setupUsingAccelStepperLib();
#endif
}

void loop() {
  // put your main code here, to run repeatedly:
#if LIB_SEL == USE_STEPPER_LIB
  loopUsingStepperLib();
#else
  loopUsingAccelStepperLib();
#endif
}
