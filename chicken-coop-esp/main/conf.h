#ifndef MAIN_CONF_H_
#define MAIN_CONF_H_

#include <cstdint>

#include "sdkconfig.h"

namespace config {

static constexpr uint32_t REVOLUTIONS_OPEN_CLOSE = 1;  // = CONFIG_CHICKEN_COOP_REVOLUTIONS;
// Duration in seconds. Minimum value: 50, Maximum value: 1000
static constexpr uint32_t DEFAULT_FULL_OPEN_CLOSE_DURATION =
    CONFIG_DEFAULT_FULL_OPEN_CLOSE_DURATION;

}  // namespace config

#endif /* MAIN_CONF_H_ */
