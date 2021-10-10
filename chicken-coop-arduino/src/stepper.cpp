#include "conf.h"

#if LIB_SEL == USE_STEPPER_LIB
void setupUsingStepperLib() {
  // Set the speed to 5 rpm. Maximum for the BYJ-48: 10-15 rpm at 5V
  MY_STEPPER.setSpeed(DEFAULT_RPM);
}

void loopUsingStepperLib() {
  digitalWrite(LED_BUILTIN, true);
  MY_STEPPER.step(DEFAULT_STEPS_PER_REV);  
  digitalWrite(LED_BUILTIN, false);
  delay(500);
  digitalWrite(LED_BUILTIN, true);
  MY_STEPPER.step(-DEFAULT_STEPS_PER_REV);  
  digitalWrite(LED_BUILTIN, false);
}
#endif