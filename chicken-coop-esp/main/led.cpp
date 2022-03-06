#include "led.h"

#include <led_strip.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *LED_TAG = "led";

#ifdef CONFIG_BLINK_LED_RMT

static led_strip_t LED_STRIP = {};

bool LED_STATE = true;

void ledTask(void *args) {
  // Configure the peripheral according to the LED type
  configureLed();

  while (true) {
    blinkLed();
    vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
  }
}

void blinkLed(void) {
  /* If the addressable LED is enabled */
  if (LED_STATE) {
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip_fill(&LED_STRIP, 0, 1, rgb_from_values(16, 16, 16));
    led_strip_flush(&LED_STRIP);
  } else {
    /* Set all LED off to clear all pixels */
    led_strip_fill(&LED_STRIP, 0, 1, rgb_from_values(0, 0, 0));
    led_strip_flush(&LED_STRIP);
  }
  LED_STATE = !LED_STATE;
}

void configureLed(void) {
  ESP_LOGI(LED_TAG, "Configuring LED");
  led_strip_install();
  LED_STRIP.gpio = static_cast<gpio_num_t>(CONFIG_BLINK_GPIO);
  LED_STRIP.type = LED_STRIP_WS2812;
  LED_STRIP.length = 1;
  LED_STRIP.channel = static_cast<rmt_channel_t>(CONFIG_BLINK_LED_RMT_CHANNEL);
  LED_STRIP.brightness = 255;

  ESP_ERROR_CHECK(led_strip_init(&LED_STRIP));
  led_strip_fill(&LED_STRIP, 0, 1, rgb_from_values(0, 0, 0));
  led_strip_flush(&LED_STRIP);
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
