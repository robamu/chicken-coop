#include "led.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *LED_TAG = "led";

#ifdef CONFIG_BLINK_LED_RMT

static led_strip_t *led_strip;
bool LED_STATE = true;

void ledTask(void *args) {
  // Configure the peripheral according to the LED type
  configureLed();

  while (true) {
    // ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
    blinkLed();
    vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
  }
}

void blinkLed(void) {
  /* If the addressable LED is enabled */
  if (LED_STATE) {
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip->set_pixel(led_strip, 0, 16, 16, 16);
    /* Refresh the strip to send data */
    led_strip->refresh(led_strip, 100);
  } else {
    /* Set all LED off to clear all pixels */
    led_strip->clear(led_strip, 50);
  }
  LED_STATE = !LED_STATE;
}

void configureLed(void) {
  ESP_LOGI(LED_TAG, "Configuring LED");
  /* LED strip initialization with the GPIO and pixels number*/
  led_strip = led_strip_init(CONFIG_BLINK_LED_RMT_CHANNEL, BLINK_GPIO, 1);
  /* Set all LED off to clear all pixels */
  led_strip->clear(led_strip, 50);
}

#elif CONFIG_BLINK_LED_GPIO

void blink_led(void) {
  /* Set the GPIO level according to the state (LOW or HIGH)*/
  gpio_set_level(BLINK_GPIO, s_led_state);
}

void configure_led(void) {
  ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
  gpio_reset_pin(BLINK_GPIO);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#endif
