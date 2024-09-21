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
static constexpr esp_log_level_t DEFAULT_LOG_LEVEL = ESP_LOG_INFO;
static constexpr int TASK_MAX_PRIORITY = configMAX_PRIORITIES - 1;

TaskHandle_t CONTROL_TASK_HANDLE = nullptr;
TaskHandle_t MOTOR_TASK_HANDLE = nullptr;
TaskHandle_t LED_TASK_HANDLE = nullptr;

// Motor MOTOR_OBJ = Motor(nullptr, nullptr);
Led LED_OBJ = Led();
Controller CONTROLLER_OBJ = Controller(LED_OBJ);
ControllerArgs CTRL_ARGS = {.controller = CONTROLLER_OBJ};
LedArgs LED_ARGS = {.led = LED_OBJ};

extern "C" void app_main(void) {
  printf("-- Chicken Coop Door Application v%d.%d.%d --\n", APP_VERSION_MAJOR, APP_VERSION_MINOR,
         APP_VERSION_REVISION);
  ESP_LOGI(APP_TAG, "Motor GPIO port mapping: Direction 0 %d | Direction 1 %d", CONFIG_MOTOR_PORT_0,
           CONFIG_MOTOR_PORT_1);
  esp_log_level_set("*", DEFAULT_LOG_LEVEL);
  motor::init();
  doorswitch::init();
  CONTROLLER_OBJ.preTaskInit();
  Controller::AppStates initState = Controller::AppStates::START_DELAY;
  if (config::START_IN_MANUAL_MODE) {
    ESP_LOGI(APP_TAG, "Starting in manual application mode");
    initState = Controller::AppStates::MANUAL;
  }
  CONTROLLER_OBJ.setAppState(initState);
  xTaskCreate(&Controller::taskEntryPoint, "Control Task", 4096, &CTRL_ARGS, TASK_MAX_PRIORITY - 1,
              &CONTROL_TASK_HANDLE);
  xTaskCreate(&Led::taskEntryPoint, "LED Task", 2048, &LED_ARGS, TASK_MAX_PRIORITY - 5,
              &LED_TASK_HANDLE);
  // This is allowed, see:
  // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/startup.html#app-main-task
  return;
}
