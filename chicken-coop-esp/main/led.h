#ifndef MAIN_LED_H_
#define MAIN_LED_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <led_strip.h>

#include <cstdint>
#include <map>

#include "sdkconfig.h"

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
 */
#define BLINK_GPIO CONFIG_BLINK_GPIO

enum class Colors {
  OFF = 0,
  WHITE_DIM = 1,
  WHITE_FULL = 2,
  GREEN = 3,
  YELLOW = 4,
};

struct LedCfg {
  Colors color;
  uint8_t brightness;
  uint32_t periodMs;
};

class Led {
 public:
  enum class LedStates { OFF = 0, BLINK_FIXED_FREQ = 1 } state = LedStates::BLINK_FIXED_FREQ;

  Colors currentColor = Colors::WHITE_DIM;

  static constexpr uint32_t BLINK_PERIOD_MANUAL = 1000;
  static constexpr uint32_t BLINK_PERIOD_MOTOR_CTRL = 500;

  Led();

  void getCurrentCfg(LedCfg& cfg) const;
  void setCurrentCfg(LedCfg cfg);
  void blinkDefault();

  static void taskEntryPoint(void* args);

 private:
  const std::map<Colors, rgb_t> COLOR_MAP = {{Colors::OFF, rgb_from_values(0, 0, 0)},
                                             {Colors::WHITE_DIM, rgb_from_values(16, 16, 16)},
                                             {Colors::WHITE_FULL, rgb_from_values(255, 255, 255)},
                                             {Colors::GREEN, rgb_from_values(0, 255, 0)},
                                             {Colors::YELLOW, rgb_from_values(255, 255, 0)}};
  void task();
  void init();

  SemaphoreHandle_t ledLock = nullptr;
  LedCfg cfg;
  uint8_t brightness = 255;
  rgb_t currentRgbValue;
  void blinkLed(LedCfg& currentCfg);

  bool ledSwitch = true;
};

struct LedArgs {
  Led& led;
};

#endif /* MAIN_LED_H_ */
