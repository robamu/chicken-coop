#ifndef MAIN_MOTOR_H_
#define MAIN_MOTOR_H_

#include <cstdint>

#include "sdkconfig.h"

enum Direction : uint8_t { CLOCK_WISE = 0, COUNTER_CLOCK_WISE = 1 };

static constexpr uint32_t BIT_MASK_OPEN = 1 << 0;
static constexpr uint32_t BIT_MASK_CLOSE = 1 << 1;

static constexpr uint32_t BIT_MASK_ALL = BIT_MASK_OPEN | BIT_MASK_CLOSE;

static constexpr uint32_t REVOLUTIONS_OPEN_CLOSE = CONFIG_CHICKEN_COOP_REVOLUTIONS;
// Duration in seconds. Minimum value: 50, Maximum value: 1000
static constexpr uint32_t DEFAULT_FULL_OPEN_CLOSE_DURATION =
    CONFIG_DEFAULT_FULL_OPEN_CLOSE_DURATION;

void motorTask(void* args);

void driveMotor(bool direction, bool print);
void driveChickenCoop(Direction direction, uint32_t stepDelayMs);
void configureDriverGpios();

#endif /* MAIN_MOTOR_H_ */
