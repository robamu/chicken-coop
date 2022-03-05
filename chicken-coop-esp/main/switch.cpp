#include "switch.h"

#include <driver/gpio.h>

#include "esp_log.h"
#include "sdkconfig.h"

gpio_config_t SWITCH_CFG = {};
static constexpr gpio_num_t SWITCH_GPIO = static_cast<gpio_num_t>(CONFIG_DOOR_SWITCH_STATE_PORT);

static constexpr char SWITCH_TAG[] = "switch";

bool switchState();

int doorswitch::init() {
  static_cast<void>(SWITCH_TAG);
  SWITCH_CFG.pin_bit_mask = 1 << CONFIG_DOOR_SWITCH_STATE_PORT;
  SWITCH_CFG.mode = GPIO_MODE_INPUT;
  ESP_ERROR_CHECK(gpio_config(&SWITCH_CFG));
  return 0;
}

bool switchState() {
#if CONFIG_INVERT_DOOR_STATE_SWITCH == 1
  return not gpio_get_level(SWITCH_GPIO);
#else
  return gpio_get_level(SWITCH_GPIO);
#endif
}

bool doorswitch::opened() { return switchState(); }

bool doorswitch::closed() { return not switchState(); }
