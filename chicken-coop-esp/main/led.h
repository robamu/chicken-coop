#ifndef MAIN_LED_H_
#define MAIN_LED_H_

#include <led_strip.h>

#include <cstdint>
#include <map>

#include "sdkconfig.h"

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
 */
#define BLINK_GPIO CONFIG_BLINK_GPIO

class Led {
 public:
  enum class LedStates { OFF = 0, BLINK_FIXED_FREQ = 1 } state = LedStates::BLINK_FIXED_FREQ;

  enum class Colors {
    OFF = 0,
    WHITE_DIM = 1,
    WHITE_FULL = 2,
    GREEN = 3,
  } currentColor = Colors::WHITE_DIM;

  static constexpr uint32_t BLINK_PERIOD_IDLE = 5000;
  static constexpr uint32_t BLINK_PERIOD_INIT = 3000;

  static constexpr uint32_t BLINK_PERIOD_MANUAL = 1000;
  static constexpr uint32_t BLINK_PERIOD_MOTOR_CTRL = 500;

  Led();

  static void taskEntryPoint(void* args);

 private:
  const std::map<Colors, rgb_t> COLOR_MAP = {{Colors::OFF, rgb_from_values(0, 0, 0)},
                                             {Colors::WHITE_DIM, rgb_from_values(16, 16, 16)},
                                             {Colors::WHITE_FULL, rgb_from_values(255, 255, 255)},
                                             {Colors::GREEN, rgb_from_values(132, 123, 23)}};
  void task();
  void init();

  uint32_t currentBlinkPeriod = 0;
  rgb_t currentRgbValue;
  void blinkLed(void);
  bool ledSwitch = true;
};

struct LedArgs {
  Led& led;
};

#endif /* MAIN_LED_H_ */
