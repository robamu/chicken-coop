#include "conf.h"

#if LIB_SEL == USE_ACCEL_STEPPER_LIB

void setupUsingAccelStepperLib() {
  MY_STEPPER.setCurrentPosition(0);  
  MY_STEPPER.setMaxSpeed(1000);
}

void loopUsingAccelStepperLib() {
  // Constant speed mode
  // Set the speed of the motor in steps per second:
  // MY_STEPPER.setSpeed(500);
  // Step the motor with constant speed as set by setSpeed():
  // MY_STEPPER.runSpeed();

  // Reset the position to 0:
  MY_STEPPER.setCurrentPosition(0);
  Serial.println("Running one revolution forward..");
  // Run the motor forward at 500 steps/second until the motor reaches 4096 steps (1 revolution):
  while (MY_STEPPER.currentPosition() != 4096) {
    MY_STEPPER.setSpeed(500);
    MY_STEPPER.runSpeed();
  }
  delay(1000);
  // Reset the position to 0:
  MY_STEPPER.setCurrentPosition(0);
  Serial.println("Running one revolution backward..");
  // Run the motor backwards at 1000 steps/second until the motor reaches -4096 steps (1 revolution):
  while (MY_STEPPER.currentPosition() != -4096) {
    MY_STEPPER.setSpeed(-1000);
    MY_STEPPER.runSpeed();
  }
  delay(1000);
  // Reset the position to 0:
  MY_STEPPER.setCurrentPosition(0);
  Serial.println("Running two revolutions forward..");
  // Run the motor forward at 1000 steps/second until the motor reaches 8192 steps (2 revolutions):
  while (MY_STEPPER.currentPosition() != 8192) {
    MY_STEPPER.setSpeed(1000);
    MY_STEPPER.runSpeed();
  }
  delay(3000);
}

#endif