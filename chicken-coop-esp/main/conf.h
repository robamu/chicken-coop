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

static constexpr uint32_t START_DELAY_MS = 4000;

static constexpr uint32_t OPEN_DURATION_MS = 150 * 1000;
static constexpr uint32_t MAX_CLOSE_DURATION = OPEN_DURATION_MS + 10 * 1000;

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
