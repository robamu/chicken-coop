#ifndef MAIN_CONF_H_
#define MAIN_CONF_H_

#include <cstdint>

#include "sdkconfig.h"

enum class Direction : uint8_t { CLOCK_WISE = 0, COUNTER_CLOCK_WISE = 1 };

namespace config {

#ifdef CONFIG_START_IN_MANUAL_MODE
static constexpr bool START_IN_MANUAL_MODE = true;
#else
static constexpr bool START_IN_MANUAL_MODE = false;
#endif

static constexpr unsigned int REVOLUTIONS_OPEN_MAX = CONFIG_CHICKEN_COOP_REVOLUTIONS;
static constexpr unsigned int REVOLUTIONS_CLOSE_MAX = REVOLUTIONS_OPEN_MAX + 1;
// Duration in seconds. Maximum value: 1000
static constexpr uint32_t DEFAULT_FULL_OPEN_CLOSE_DURATION =
    CONFIG_DEFAULT_FULL_OPEN_CLOSE_DURATION;

using OpenCloseToDirCb = Direction (*)(bool open);
using StopConditionArgs = void*;
using StopConditionCb = bool (*)(Direction dir, uint8_t revolutionIdx, StopConditionArgs args);

}  // namespace config

#endif /* MAIN_CONF_H_ */
