#ifndef MAIN_LED_H_
#define MAIN_LED_H_

#include "sdkconfig.h"

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
 */
#define BLINK_GPIO CONFIG_BLINK_GPIO

void blinkLed(void);
void configureLed(void);

#endif /* MAIN_LED_H_ */
