#include "led.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *LED_TAG = "led";

static led_strip_t LED_STRIP = {};

Led::Led() {}

void Led::taskEntryPoint(void *args) {
  LedArgs *ledArgs = reinterpret_cast<LedArgs *>(args);
  if (ledArgs != nullptr) {
    ledArgs->led.task();
  }
}

void Led::init(void) {
  ESP_LOGI(LED_TAG, "Configuring LED");
  currentBlinkPeriod = BLINK_PERIOD_IDLE;
  led_strip_install();
  LED_STRIP.gpio = static_cast<gpio_num_t>(CONFIG_BLINK_GPIO);
  LED_STRIP.type = LED_STRIP_WS2812;
  LED_STRIP.length = 1;
  LED_STRIP.channel = static_cast<rmt_channel_t>(CONFIG_BLINK_LED_RMT_CHANNEL);
  LED_STRIP.brightness = 255;

  ESP_ERROR_CHECK(led_strip_init(&LED_STRIP));
  currentRgbValue = COLOR_MAP.at(Colors::OFF);
  led_strip_fill(&LED_STRIP, 0, 1, currentRgbValue);
  led_strip_flush(&LED_STRIP);
}

void Led::blinkLed(void) {
  /* If the addressable LED is enabled */
  if (ledSwitch) {
    currentRgbValue = COLOR_MAP.at(currentColor);
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip_fill(&LED_STRIP, 0, 1, currentRgbValue);
    led_strip_flush(&LED_STRIP);
  } else {
    currentRgbValue = COLOR_MAP.at(Colors::OFF);
    /* Set all LED off to clear all pixels */
    led_strip_fill(&LED_STRIP, 0, 1, currentRgbValue);
    led_strip_flush(&LED_STRIP);
  }
  ledSwitch = not ledSwitch;
}

void Led::task() {
  // Configure the peripheral according to the LED type
  init();

  while (true) {
    blinkLed();
    vTaskDelay(currentBlinkPeriod / portTICK_PERIOD_MS);
  }
}
