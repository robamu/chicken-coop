/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static constexpr gpio_num_t STEPPER_IN1 = GPIO_NUM_4;
static constexpr gpio_num_t STEPPER_IN2 = GPIO_NUM_5;
static constexpr gpio_num_t STEPPER_IN3 = GPIO_NUM_6;
static constexpr gpio_num_t STEPPER_IN4 = GPIO_NUM_7;

const uint8_t STEP_SEQUENCE[8] = {
		0b0001,
		0b0101,
		0b0100,
		0b0110,
		0b0010,
		0b1010,
		0b1000,
		0b1001
};

static uint32_t STEP_COUNTER = 0;
const uint32_t STEP_COUNT = 2048;
const bool DIRECTION = true;

const uint32_t STEP_DELAY = 2;

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

static void configure_led(void)
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
	gpio_reset_pin(STEPPER_IN1);
	gpio_reset_pin(STEPPER_IN2);
	gpio_reset_pin(STEPPER_IN3);
	gpio_reset_pin(STEPPER_IN4);

	gpio_set_direction(STEPPER_IN1, GPIO_MODE_OUTPUT);
	gpio_set_direction(STEPPER_IN2, GPIO_MODE_OUTPUT);
	gpio_set_direction(STEPPER_IN3, GPIO_MODE_OUTPUT);
	gpio_set_direction(STEPPER_IN4, GPIO_MODE_OUTPUT);

	gpio_set_level(STEPPER_IN1, false);
	gpio_set_level(STEPPER_IN2, false);
	gpio_set_level(STEPPER_IN3, false);
	gpio_set_level(STEPPER_IN4, false);
}


void driveMotor() {
    for(uint32_t idx = 0; idx < STEP_COUNT; idx++) {
		uint8_t currentSeq = STEP_SEQUENCE[MOTOR_STEP_COUNTER];
		// ESP_LOGI(TAG, "Current sequence for motor step %d: %d",
		// 		MOTOR_STEP_COUNTER, currentSeq);
    	for(uint8_t motorIdx = 0; motorIdx < 4; motorIdx ++) {
    		bool setHigh = (currentSeq >> (3 - motorIdx)) & 0x01;
    		// ESP_LOGI(TAG, "Set high: %d", (int) setHigh);
    		gpio_set_level(MOTOR_PINS[motorIdx], setHigh);
    	}
    	if(DIRECTION) {
    		MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + (8 - 1)) % 8;
    	} else {
    		MOTOR_STEP_COUNTER = (MOTOR_STEP_COUNTER + 1) % 8;
    	}
    	vTaskDelay(STEP_DELAY / portTICK_PERIOD_MS);
    }
}

extern "C" void app_main(void)
{

    // Configure the peripheral according to the LED type
    configure_led();

    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        // Toggle the LED state
        s_led_state = !s_led_state;

        // Handle motor driver code
        driveMotor();

        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
