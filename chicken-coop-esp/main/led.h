#ifndef MAIN_LED_H_
#define MAIN_LED_H_

#include <cstdint>

#include "sdkconfig.h"

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
 */
#define BLINK_GPIO CONFIG_BLINK_GPIO

static constexpr uint32_t BLINK_PERIOD_IDLE = 5000;
static constexpr uint32_t BLINK_PERIOD_INIT = 3000;

static constexpr uint32_t BLINK_PERIOD_MANUAL = 1000;
static constexpr uint32_t BLINK_PERIOD_MOTOR_CTRL = 500;

void ledTask(void* args);

void blinkLed(void);
void configureLed(void);

#endif /* MAIN_LED_H_ */
