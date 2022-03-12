#include "motor.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "sdkconfig.h"

Motor::Motor(uint32_t fullOpenCloseDuration, uint32_t revolutionsOpenClose)
    : fullOpenCloseDuration(fullOpenCloseDuration), revolutionsOpenClose(revolutionsOpenClose) {}

void Motor::taskEntryPoint(void* args) {
  MotorArgs* motorArgs = reinterpret_cast<MotorArgs*>(args);
  if (motorArgs != nullptr) {
    motorArgs->motor.taskHandle = xTaskGetCurrentTaskHandle();
    motorArgs->motor.taskOp();
  }
}

void Motor::taskOp() {
  configureDriverGpios();
  if (fullOpenCloseDuration < 50 or fullOpenCloseDuration > 1000) {
    ESP_LOGW(MOTOR_TAG, "Invalid open close duration of %d seconds", fullOpenCloseDuration);
    ESP_LOGW(MOTOR_TAG, "Minimum is 50 seconds, maximum is 1000 seconds. Assuming 120 seconds");
    fullOpenCloseDuration = 120;
  }
  stepDelayMs = fullOpenCloseDuration * 1000 / 12 / 4096;
  ESP_LOGI(MOTOR_TAG, "Calculated delay between steps: %d milliseconds", stepDelayMs);
  while (true) {
    // Wait for the control task to notify the motor task
    BaseType_t retval = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (retval == pdPASS) {
      // Handle motor driver code
      driveChickenCoop();
      driverState = DriverStates::IDLE;
      // Go into blocked state again
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
  }
}

void Motor::configureDriverGpios() {
  // zero-initialize the config structure.
  gpio_config_t io_conf = {};

  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = GPIO_MASK;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);

  gpio_set_level(STEPPER_IN1, false);
  gpio_set_level(STEPPER_IN2, false);
  gpio_set_level(STEPPER_IN3, false);
  gpio_set_level(STEPPER_IN4, false);
}

void Motor::printMotorDrive(bool direction) {
  const char* dirString = nullptr;
  if (direction) {
    dirString = "clockwise";
  } else {
    dirString = "counter-clockwise";
  }
  ESP_LOGI(MOTOR_TAG, "Driving motor %s for one revolution(s)", dirString);
}

void Motor::driveMotor(bool print) {
  if (print) {
    printMotorDrive(direction);
  }
  for (uint32_t idx = 0; idx < STEP_COUNT; idx++) {
    uint8_t currentSeq = STEP_SEQUENCE[MOTOR_STEP_COUNTER];
    // ESP_LOGI(TAG, "Current sequence for motor step %d: %d",
    //    MOTOR_STEP_COUNTER, currentSeq);
    for (uint8_t motorIdx = 0; motorIdx < 4; motorIdx++) {
      bool setHigh = (currentSeq >> (3 - motorIdx)) & 0x01;
      // ESP_LOGI(TAG, "Set high: %d", (int) setHigh);
      gpio_set_level(MOTOR_PINS[motorIdx], setHigh);
    }
    if (direction == Direction::CLOCK_WISE) {
      MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + (8 - 1)) % 8;
    } else if (direction == Direction::COUNTER_CLOCK_WISE) {
      MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + 1) % 8;
    }
    if (idx % 124 == 0) {
      esp_task_wdt_reset();
    }
    vTaskDelay(stepDelayMs / portTICK_PERIOD_MS);
  }
}

void Motor::driveChickenCoop() {
  const char* dirString = nullptr;
  if (direction == Direction::CLOCK_WISE) {
    dirString = "clockwise";
  } else if (direction == Direction::COUNTER_CLOCK_WISE) {
    dirString = "counter-clockwise";
  }
  ESP_LOGI(MOTOR_TAG, "Driving motor %s for %d revolution(s)", dirString, revolutionsOpenClose);
  for (uint8_t idx = 0; idx < revolutionsOpenClose; idx++) {
    driveMotor(false);
  }
}

bool Motor::requestOpen() {
  if (driverState != DriverStates::IDLE) {
    return false;
  }
#if CONFIG_CLOCKWISE_IS_OPEN == 1
  direction = Direction::CLOCK_WISE;
#else
  direction = Direction::COUNTER_CLOCK_WISE;
#endif
  opPending = true;
  driverState = DriverStates::OPENING;
  // Unblocks motor task
  xTaskNotifyGive(taskHandle);
  return true;
}

bool Motor::requestClose() {
  if (driverState != DriverStates::IDLE) {
    return false;
  }
#if CONFIG_CLOCKWISE_IS_OPEN == 1
  direction = Direction::COUNTER_CLOCK_WISE;
#else
  direction = Direction::CLOCK_WISE;
#endif
  opPending = true;
  driverState = DriverStates::CLOSING;
  // Unblocks motor task
  xTaskNotifyGive(taskHandle);
  return true;
}

bool Motor::operationDone() {
  if (opPending and driverState == DriverStates::IDLE) {
    opPending = false;
    return true;
  }
  return false;
}
