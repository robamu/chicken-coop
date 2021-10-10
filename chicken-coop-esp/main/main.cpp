/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

#include <cstdio>

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
 */
#define BLINK_GPIO CONFIG_BLINK_GPIO

static constexpr gpio_num_t STEPPER_IN1 = GPIO_NUM_4;
static constexpr gpio_num_t STEPPER_IN2 = GPIO_NUM_5;
static constexpr gpio_num_t STEPPER_IN3 = GPIO_NUM_6;
static constexpr gpio_num_t STEPPER_IN4 = GPIO_NUM_7;

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
static bool DIRECTION = true;

const uint32_t STEP_DELAY = 5;

const gpio_num_t MOTOR_PINS[4] = {
    STEPPER_IN1,
    STEPPER_IN3,
    STEPPER_IN2,
    STEPPER_IN4
};
static uint8_t MOTOR_STEP_COUNTER = 0;

static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_RMT
static led_strip_t *pStrip_a;

static void blink_led(void)
{
  /* If the addressable LED is enabled */
  if (s_led_state) {
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    pStrip_a->set_pixel(pStrip_a, 0, 16, 16, 16);
    /* Refresh the strip to send data */
    pStrip_a->refresh(pStrip_a, 100);
  } else {
    /* Set all LED off to clear all pixels */
    pStrip_a->clear(pStrip_a, 50);
  }
}

static void configureLed(void)
{
  ESP_LOGI(TAG, "Example configured to blink addressable LED!");
  /* LED strip initialization with the GPIO and pixels number*/
  pStrip_a = led_strip_init(CONFIG_BLINK_LED_RMT_CHANNEL, BLINK_GPIO, 1);
  /* Set all LED off to clear all pixels */
  pStrip_a->clear(pStrip_a, 50);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
  /* Set the GPIO level according to the state (LOW or HIGH)*/
  gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
  ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
  gpio_reset_pin(BLINK_GPIO);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#endif

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

void ledTask(void* args) {
  // Configure the peripheral according to the LED type
  configureLed();

  while(true) {
    // ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
    blink_led();
    // Toggle the LED state
    s_led_state = !s_led_state;
    vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
  }
}

void driveMotor(bool direction) {
  ESP_LOGI("motor", "Driving motor..");
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
    vTaskDelay(STEP_DELAY / portTICK_PERIOD_MS);
  }
}

void motorTask(void* args) {
  esp_task_wdt_add(nullptr);
  configureDriverGpios();

  while (1) {
    DIRECTION = !DIRECTION;
    // Handle motor driver code
    driveMotor(DIRECTION);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

extern "C" void app_main(void)
{
  xTaskCreate(motorTask, "Motor Task", 2048, nullptr, 0, nullptr);
  ledTask(nullptr);
}
