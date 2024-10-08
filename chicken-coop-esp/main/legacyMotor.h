#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstdint>

#include "conf.h"
#include "driver/gpio.h"
#include "motorDefs.h"

/// @brief Driver for the 28BYJ-48
class LegacyMotor {
 public:
  static constexpr uint32_t BIT_MASK_OPEN = 1 << 0;
  static constexpr uint32_t BIT_MASK_CLOSE = 1 << 1;

  static constexpr uint32_t BIT_MASK_ALL = BIT_MASK_OPEN | BIT_MASK_CLOSE;

  LegacyMotor(uint32_t fullOpenCloseDuration, uint32_t revolutionsOpenClose,
              config::StopConditionCb stopCb, config::StopConditionArgs stopArgs,
              config::OpenCloseToDirCb dirMapper);

  void setStopConditionCb(config::StopConditionCb cb, config::StopConditionArgs stopArgs);
  void setDirectionMapper(config::OpenCloseToDirCb mapper);

  static void taskEntryPoint(void* args);

  bool requestOpen();
  bool requestClose();
  bool operationDone();

 private:
  static constexpr gpio_num_t STEPPER_IN1 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN1_PORT);
  static constexpr gpio_num_t STEPPER_IN2 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN2_PORT);
  static constexpr gpio_num_t STEPPER_IN3 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN3_PORT);
  static constexpr gpio_num_t STEPPER_IN4 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN4_PORT);

  static constexpr uint64_t GPIO_MASK =
      (1ULL << STEPPER_IN1) | (1ULL << STEPPER_IN2) | (1ULL << STEPPER_IN3) | (1ULL << STEPPER_IN4);
  static constexpr char MOTOR_TAG[] = "motor";

  const uint8_t STEP_SEQUENCE[8] = {0b1000, 0b1010, 0b0010, 0b0110, 0b0100, 0b0101, 0b0001, 0b1001};

  const uint32_t STEP_COUNT = 4096;

  const gpio_num_t MOTOR_PINS[4] = {STEPPER_IN1, STEPPER_IN3, STEPPER_IN2, STEPPER_IN4};
  uint8_t MOTOR_STEP_COUNTER = 0;
  SemaphoreHandle_t motorLock = nullptr;
  config::StopConditionCb stopCb = nullptr;
  config::StopConditionArgs stopArgs = nullptr;
  config::OpenCloseToDirCb dirMapper = nullptr;

  Direction direction = Direction::CLOCK_WISE;
  TaskHandle_t taskHandle = nullptr;
  unsigned fullOpenCloseDuration = 0;
  unsigned revolutionsMax = 0;
  unsigned defaultFullOpenCloseDuration = 0;
  unsigned stepDelayMs = 0;
  bool opPending = false;

  void taskOp();
  bool driveMotorOneRevolution(uint8_t revolutionIdx, bool print);
  void driveChickenCoop();
  void configureDriverGpios();
  void printMotorDrive(Direction direction);
};

struct MotorArgs {
  LegacyMotor& motor;
};