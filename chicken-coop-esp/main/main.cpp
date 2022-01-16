#include "main.h"

#include <cstdio>

#include "control.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "led.h"
#include "motor.h"
#include "open_close_times.h"
#include "sdkconfig.h"
#include "usr_config.h"

static const char APP_TAG[] = "chicken-coop";

static constexpr int TASK_MAX_PRIORITY = configMAX_PRIORITIES - 1;

extern "C" void app_main(void) {
  ESP_LOGI(APP_TAG, "-- Chicken Coop Door Application v%d.%d.%d--", APP_VERSION_MAJOR,
           APP_VERSION_MINOR, APP_VERSION_REVISION);
  ESP_LOGI(APP_TAG, "INx to GPIO port mapping: IN1 -> %d | IN2 -> %d | IN3 -> %d | IN4 -> %d",
           CONFIG_STEPPER_IN1_PORT, CONFIG_STEPPER_IN2_PORT, CONFIG_STEPPER_IN3_PORT,
           CONFIG_STEPPER_IN4_PORT);
  xTaskCreate(&Controller::taskEntryPoint, "Control Task", 1024, nullptr, TASK_MAX_PRIORITY - 1,
              &controlTaskHandle);
  xTaskCreate(motorTask, "Motor Task", 2048, nullptr, TASK_MAX_PRIORITY - 3, &motorTaskHandle);
  xTaskCreate(ledTask, "LED Task", 1024, nullptr, TASK_MAX_PRIORITY - 5, &ledTaskHandle);
  // This is allowed, see:
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/startup.html#app-main-task
  return;
}
