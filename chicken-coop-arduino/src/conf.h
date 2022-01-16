#ifndef _CONF_H_
#define _CONF_H_

#include <cstdint>

#define DS1307_RTC_CONNECTED    1
#define MOTOR_CONNECTED         0
// Set this to 1 to reload the time on the RTC to the current time
// The compile time of this script will be used as the current time
#define FORCE_TIME_RELOAD       1

// User configuration defines
#define USE_STEPPER_LIB         0
#define USE_ACCEL_STEPPER_LIB   1
#define LIB_SEL                 USE_ACCEL_STEPPER_LIB

// For full-step mode, this will be one full rotation
static constexpr int DEFAULT_STEPS_PER_REV =    2048;
static constexpr uint32_t DEFAULT_RPM =         5;

static constexpr uint8_t PIN_IN1 = 2;
static constexpr uint8_t PIN_IN2 = 3;
static constexpr uint8_t PIN_IN3 = 4;
static constexpr uint8_t PIN_IN4 = 5;

#if LIB_SEL == USE_STEPPER_LIB
// Install the Stepper library provided by Arduino first
#include <Stepper.h>
extern Stepper MY_STEPPER;
#else
#include <AccelStepper.h>
extern AccelStepper MY_STEPPER;
#endif

enum MotorInterfaceType: uint8_t {
  HALF_STEP = AccelStepper::HALF4WIRE,
  FULL_STEP = AccelStepper::FULL4WIRE
};

static constexpr MotorInterfaceType MOTOR_INTERFACE_TYPE = 
    MotorInterfaceType::HALF_STEP;

#endif /* _CONF_H_ */