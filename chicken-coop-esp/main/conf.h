#ifndef MAIN_CONF_H_
#define MAIN_CONF_H_

#include <cstdint>

#include "sdkconfig.h"

enum class Direction : uint8_t { CLOCK_WISE = 0, COUNTER_CLOCK_WISE = 1 };

namespace config {

static constexpr bool START_IN_MANUAL_MODE = CONFIG_START_IN_MANUAL_MODE;

static constexpr uint32_t REVOLUTIONS_MAX = CONFIG_CHICKEN_COOP_REVOLUTIONS;
// Duration in seconds. Maximum value: 1000
static constexpr uint32_t DEFAULT_FULL_OPEN_CLOSE_DURATION =
    CONFIG_DEFAULT_FULL_OPEN_CLOSE_DURATION;

using OpenCloseToDirCb = Direction (*)(bool open);
using StopConditionArgs = void*;
using StopConditionCb = bool (*)(Direction dir, uint8_t revolutionIdx, StopConditionArgs args);

}  // namespace config

#endif /* MAIN_CONF_H_ */
