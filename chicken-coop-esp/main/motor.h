#pragma once

#include "conf.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

namespace motor {

static constexpr gpio_num_t DIR_0_PIN = static_cast<gpio_num_t>(CONFIG_MOTOR_PORT_0);
static constexpr gpio_num_t DIR_1_PIN = static_cast<gpio_num_t>(CONFIG_MOTOR_PORT_1);
static constexpr uint64_t GPIO_MASK = (1ULL << DIR_0_PIN) | (1ULL << DIR_1_PIN);

void init();

void driveDir(bool dir);
void driveDir0();
void driveDir1();
void stop();

}  // namespace motor