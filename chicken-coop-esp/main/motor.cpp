#include "motor.h"

#include "esp_log.h"
#include "motorDefs.h"

void motor::init() {
  // zero-initialize the config structure.
  gpio_config_t io_conf = {};

  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = GPIO_MASK;
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  gpio_config(&io_conf);

  gpio_set_level(DIR_0_PIN, 0);
  gpio_set_level(DIR_1_PIN, 0);
}

void motor::driveDir(bool dir) {
  if (dir) {
    driveDir1();
  } else {
    driveDir0();
  }
}

void motor::driveDir0() {
  gpio_set_level(DIR_0_PIN, 1);
  gpio_set_level(DIR_1_PIN, 0);
}

void motor::driveDir1() {
  gpio_set_level(DIR_0_PIN, 0);
  gpio_set_level(DIR_1_PIN, 1);
}

void motor::stop() {
  gpio_set_level(DIR_0_PIN, 0);
  gpio_set_level(DIR_1_PIN, 0);
}