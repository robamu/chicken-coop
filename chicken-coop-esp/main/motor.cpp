#include "motor.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr gpio_num_t STEPPER_IN1 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN1_PORT);
static constexpr gpio_num_t STEPPER_IN2 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN2_PORT);
static constexpr gpio_num_t STEPPER_IN3 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN3_PORT);
static constexpr gpio_num_t STEPPER_IN4 = static_cast<gpio_num_t>(CONFIG_STEPPER_IN4_PORT);

static constexpr uint64_t GPIO_MASK = (1ULL << STEPPER_IN1) | (1ULL << STEPPER_IN2) |
    (1ULL << STEPPER_IN3) | (1ULL << STEPPER_IN4);

const uint8_t STEP_SEQUENCE[8] = {
    0b1000,
    0b1010,
    0b0010,
    0b0110,
    0b0100,
    0b0101,
    0b0001,
    0b1001
};

const uint32_t STEP_COUNT = 4096;

const gpio_num_t MOTOR_PINS[4] = {
    STEPPER_IN1,
    STEPPER_IN3,
    STEPPER_IN2,
    STEPPER_IN4
};
static uint8_t MOTOR_STEP_COUNTER = 0;

void configureDriverGpios() {
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

void printMotorDrive(bool direction) {
  const char* dirString = nullptr;
  if(direction) {
    dirString = "clockwise";
  } else {
    dirString = "counter-clockwise";
  }
  ESP_LOGI(MOTOR_TAG, "Driving motor %s for one revolution", dirString);
}

void driveMotor(bool direction, bool print, uint32_t stepDelayMs) {
  if(print) {
    printMotorDrive(direction);
  }
  for(uint32_t idx = 0; idx < STEP_COUNT; idx++) {
    uint8_t currentSeq = STEP_SEQUENCE[MOTOR_STEP_COUNTER];
    // ESP_LOGI(TAG, "Current sequence for motor step %d: %d",
    //    MOTOR_STEP_COUNTER, currentSeq);
    for(uint8_t motorIdx = 0; motorIdx < 4; motorIdx ++) {
      bool setHigh = (currentSeq >> (3 - motorIdx)) & 0x01;
      // ESP_LOGI(TAG, "Set high: %d", (int) setHigh);
      gpio_set_level(MOTOR_PINS[motorIdx], setHigh);
    }
    if(direction) {
      MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + (8 - 1)) % 8;
    } else {
      MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + 1) % 8;
    }
    if(idx % 124 == 0) {
      esp_task_wdt_reset();
    }
    vTaskDelay(stepDelayMs / portTICK_PERIOD_MS);
  }
}

void driveChickenCoop(bool direction, uint32_t stepDelayMs) {
  const char* dirString = nullptr;
  if(direction) {
    dirString = "clockwise";
  } else {
    dirString = "counter-clockwise";
  }
  ESP_LOGI(MOTOR_TAG, "Driving motor %s for %d revolutions", dirString,
      REVOLUTIONS_OPEN_CLOSE);
  for(uint8_t idx = 0; idx < 12; idx++) {
    driveMotor(direction, false, stepDelayMs);
  }
}
