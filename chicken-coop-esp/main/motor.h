#ifndef MAIN_MOTOR_H_
#define MAIN_MOTOR_H_

#include <cstdint>

#include "sdkconfig.h"

static constexpr uint32_t REVOLUTIONS_OPEN_CLOSE = CONFIG_CHICKEN_COOP_REVOLUTIONS;
// Duration in seconds. Minimum value: 50, Maximum value: 1000
static constexpr uint32_t DEFAULT_FULL_OPEN_CLOSE_DURATION =
    CONFIG_DEFAULT_FULL_OPEN_CLOSE_DURATION;

void motorTask(void* args);

void driveMotor(bool direction, bool print);
void driveChickenCoop(bool direction, uint32_t stepDelayMs);
void configureDriverGpios();

#endif /* MAIN_MOTOR_H_ */
