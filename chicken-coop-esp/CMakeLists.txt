# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/led_strip)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(chicken-coop)

set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 1)
set(APP_VERSION_REVISION 0)

configure_file(main/usr_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/usr_config.h)
