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

// Duration in seconds. Maximum value: 1000
static constexpr uint32_t DEFAULT_FULL_OPEN_CLOSE_DURATION =
    CONFIG_DEFAULT_FULL_OPEN_CLOSE_DURATION;

using OpenCloseToDirCb = Direction (*)(bool open);
using StopConditionArgs = void*;
using StopConditionCb = bool (*)(Direction dir, StopConditionArgs args);

static inline Direction dirMapper(bool close) {
  Direction dir = Direction::CLOCK_WISE;
  if (close) {
#if CONFIG_CLOCKWISE_IS_OPEN == 1
    dir = Direction::CLOCK_WISE;
#else
    dir = Direction::COUNTER_CLOCK_WISE;
#endif
  } else {
#if CONFIG_CLOCKWISE_IS_OPEN == 1
    dir = Direction::COUNTER_CLOCK_WISE;
#else
    dir = Direction::CLOCK_WISE;
#endif
  }
  return dir;
}

}  // namespace config

#endif /* MAIN_CONF_H_ */
