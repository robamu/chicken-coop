#include "led.h"
#include "motor.h"
#include "main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_task_wdt.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include <cstdio>

static const char* APP_TAG = "chicken-coop";

static bool DIRECTION = true;

void ledTask(void* args);
void motorTask(void* args);

extern "C" void app_main(void)
{
  ESP_LOGI(APP_TAG, "-- Chicken Coop Door Application v%d.%d.%d--", APP_VERSION_MAJOR,
      APP_VERSION_MINOR, APP_VERSION_REVISION);
  ESP_LOGI(APP_TAG, "INx to GPIO port mapping: IN1 -> %d | IN2 -> %d | IN3 -> %d | IN4 -> %d",
      CONFIG_STEPPER_IN1_PORT, CONFIG_STEPPER_IN2_PORT,
      CONFIG_STEPPER_IN3_PORT, CONFIG_STEPPER_IN4_PORT);
  xTaskCreate(motorTask, "Motor Task", 2048, nullptr, 0, nullptr);
  ledTask(nullptr);
}

void motorTask(void* args) {
  esp_task_wdt_add(nullptr);
  configureDriverGpios();
  uint32_t fullOpenCloseDuration = DEFAULT_FULL_OPEN_CLOSE_DURATION;
  if (DEFAULT_FULL_OPEN_CLOSE_DURATION < 50 or DEFAULT_FULL_OPEN_CLOSE_DURATION > 1000) {
    ESP_LOGW(MOTOR_TAG, "Invalid open close duration of %d seconds",
        DEFAULT_FULL_OPEN_CLOSE_DURATION);
    ESP_LOGW(MOTOR_TAG, "Minimum is 50 seconds, maximum is 1000 seconds. Assuming 120 seconds");
    fullOpenCloseDuration = 120;
  }
  uint32_t stepDelayMs = fullOpenCloseDuration * 1000 / 12 / 4096;
  ESP_LOGI(MOTOR_TAG, "Calculated delay between steps: %d milliseconds", stepDelayMs);
  while (1) {
    DIRECTION = !DIRECTION;
    // Handle motor driver code
    driveChickenCoop(DIRECTION, stepDelayMs);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void ledTask(void* args) {
  // Configure the peripheral according to the LED type
  configureLed();

  while(true) {
    // ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
    blinkLed();
    vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
  }
}
