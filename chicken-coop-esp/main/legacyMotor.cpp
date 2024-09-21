#include "esp_log.h"
#include "esp_task_wdt.h"
#include "motor.h"
#include "rom/ets_sys.h"
#include "sdkconfig.h"

LegacyMotor::LegacyMotor(uint32_t fullOpenCloseDuration, uint32_t revolutionsMax,
                         config::StopConditionCb stopCb, config::StopConditionArgs stopArgs,
                         config::OpenCloseToDirCb dirMapper)
    : stopCb(stopCb),
      stopArgs(stopArgs),
      dirMapper(dirMapper),
      fullOpenCloseDuration(fullOpenCloseDuration),
      revolutionsMax(revolutionsMax) {
  motorLock = xSemaphoreCreateMutex();
  if (motorLock != nullptr) {
    xSemaphoreGive(motorLock);
  }
}

void LegacyMotor::taskEntryPoint(void* args) {
  MotorArgs* motorArgs = reinterpret_cast<MotorArgs*>(args);
  if (motorArgs != nullptr) {
    motorArgs->motor.taskHandle = xTaskGetCurrentTaskHandle();
    motorArgs->motor.taskOp();
  }
}

void LegacyMotor::taskOp() {
  configureDriverGpios();
  if (fullOpenCloseDuration > 1000) {
    ESP_LOGW(MOTOR_TAG, "Invalid open close duration of %d seconds", fullOpenCloseDuration);
    ESP_LOGW(MOTOR_TAG, "Maximum is 1000 seconds. Assuming 60 seconds");
    fullOpenCloseDuration = 60;
  }
  unsigned stepDelayUs = fullOpenCloseDuration * 1000 * 1000 / 12 / 4096;
  if (stepDelayUs <= 1000) {
    ESP_LOGI(MOTOR_TAG,
             "Calculated delay between steps is less than 1 millisecond with %d microseconds",
             stepDelayUs);
    ESP_LOGI(MOTOR_TAG, "Rounding to 1 millisecond delay between steps");
    stepDelayMs = 1;
  } else {
    stepDelayMs = stepDelayUs / 1000;
  }
  ESP_LOGI(MOTOR_TAG, "Calculated delay between steps: %d milliseconds", stepDelayMs);
  BaseType_t retval = 0;
  while (true) {
    // Wait for the control task to notify the motor task
    retval = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (retval == pdPASS) {
      // Handle motor driver code
      driveChickenCoop();
      xSemaphoreTake(motorLock, portMAX_DELAY);
      driverState = DriverStates::IDLE;
      ESP_LOGD(MOTOR_TAG, "Motor operation done");
      xSemaphoreGive(motorLock);
    }
  }
}

void LegacyMotor::configureDriverGpios() {
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

void LegacyMotor::printMotorDrive(Direction direction) {
  const char* dirString = nullptr;
  if (direction == Direction::CLOCK_WISE) {
    dirString = "clockwise";
  } else {
    dirString = "counter-clockwise";
  }
  ESP_LOGI(MOTOR_TAG, "Driving motor %s for one revolution(s)", dirString);
}

bool LegacyMotor::driveMotorOneRevolution(uint8_t revolutionIdx, bool print) {
  if (print) {
    printMotorDrive(direction);
  }
  bool stopCondReached = false;
  for (uint32_t idx = 0; idx < STEP_COUNT; idx++) {
    uint8_t currentSeq = STEP_SEQUENCE[MOTOR_STEP_COUNTER];
    for (uint8_t motorIdx = 0; motorIdx < 4; motorIdx++) {
      bool setHigh = (currentSeq >> (3 - motorIdx)) & 0x01;
      gpio_set_level(MOTOR_PINS[motorIdx], setHigh);
    }
    if (direction == Direction::CLOCK_WISE) {
      MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + (8 - 1)) % 8;
    } else if (direction == Direction::COUNTER_CLOCK_WISE) {
      MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + 1) % 8;
    }
    if (idx % 124 == 0) {
      esp_task_wdt_reset();
      if (stopCb(direction, revolutionIdx, stopArgs)) {
        stopCondReached = true;
        break;
      }
    }
    vTaskDelay(stepDelayMs / portTICK_PERIOD_MS);
  }
  return stopCondReached;
}

void LegacyMotor::driveChickenCoop() {
  const char* dirString = nullptr;
  if (direction == Direction::CLOCK_WISE) {
    dirString = "clockwise";
  } else if (direction == Direction::COUNTER_CLOCK_WISE) {
    dirString = "counter-clockwise";
  }
  ESP_LOGI(MOTOR_TAG, "Driving motor %s for maximum of %d revolution(s)", dirString,
           revolutionsMax);
  for (uint8_t idx = 0; idx < revolutionsMax; idx++) {
    bool stopCond = driveMotorOneRevolution(idx, false);
    if (stopCond) {
      ESP_LOGI(MOTOR_TAG, "Stop condition reached. Motor operation done");
      break;
    }
  }
}

bool LegacyMotor::requestOpen() {
  bool result = false;
  xSemaphoreTake(motorLock, portMAX_DELAY);
  if (driverState == DriverStates::IDLE) {
    direction = dirMapper(false);
    opPending = true;
    result = true;
    driverState = DriverStates::OPENING;
    // Unblocks motor task
    xTaskNotifyGive(taskHandle);
  }
  xSemaphoreGive(motorLock);
  return result;
}

bool LegacyMotor::requestClose() {
  bool result = false;
  xSemaphoreTake(motorLock, portMAX_DELAY);
  if (driverState == DriverStates::IDLE) {
    direction = dirMapper(true);
    opPending = true;
    result = true;
    driverState = DriverStates::CLOSING;
    // Unblocks motor task
    xTaskNotifyGive(taskHandle);
  }
  xSemaphoreGive(motorLock);
  return result;
}

bool LegacyMotor::operationDone() {
  bool result = false;
  if (not opPending) {
    result = true;
  } else {
    xSemaphoreTake(motorLock, portMAX_DELAY);
    if (driverState == DriverStates::IDLE) {
      opPending = false;
      result = true;
    }
    xSemaphoreGive(motorLock);
  }
  return result;
}
void LegacyMotor::setStopConditionCb(config::StopConditionCb cb,
                                     config::StopConditionArgs stopArgs) {
  this->stopCb = cb;
  this->stopArgs = stopArgs;
}

void LegacyMotor::setDirectionMapper(config::OpenCloseToDirCb mapper) { this->dirMapper = mapper; }
