#include "main.h"

#include <cstdio>

#include "control.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "led.h"
#include "motor.h"
#include "open_close_times.h"
#include "sdkconfig.h"
#include "switch.h"
#include "usr_config.h"

static const char APP_TAG[] = "chicken-coop";

static constexpr int TASK_MAX_PRIORITY = configMAX_PRIORITIES - 1;

TaskHandle_t CONTROL_TASK_HANDLE = nullptr;
TaskHandle_t MOTOR_TASK_HANDLE = nullptr;
TaskHandle_t LED_TASK_HANDLE = nullptr;

extern "C" void app_main(void) {
  ESP_LOGI("", "-- Chicken Coop Door Application v%d.%d.%d--", APP_VERSION_MAJOR, APP_VERSION_MINOR,
           APP_VERSION_REVISION);
  ESP_LOGI(APP_TAG, "INx to GPIO port mapping: IN1 -> %d | IN2 -> %d | IN3 -> %d | IN4 -> %d",
           CONFIG_STEPPER_IN1_PORT, CONFIG_STEPPER_IN2_PORT, CONFIG_STEPPER_IN3_PORT,
           CONFIG_STEPPER_IN4_PORT);
  doorswitch::init();
  Controller::uartInit();
  xTaskCreate(&Controller::taskEntryPoint, "Control Task", 4096, nullptr, TASK_MAX_PRIORITY - 1,
              &CONTROL_TASK_HANDLE);
  xTaskCreate(motorTask, "Motor Task", 2048, nullptr, TASK_MAX_PRIORITY - 3, &MOTOR_TASK_HANDLE);
  xTaskCreate(ledTask, "LED Task", 2048, nullptr, TASK_MAX_PRIORITY - 5, &LED_TASK_HANDLE);
  // This is allowed, see:
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/startup.html#app-main-task
  return;
}
