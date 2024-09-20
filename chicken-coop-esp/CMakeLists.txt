# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.11)

include(FetchContent)

# Make available additional drivers for the DS3231 RTC device
FetchContent_Declare(
  espidflib
  GIT_REPOSITORY https://github.com/UncleRus/esp-idf-lib.git
)
FetchContent_MakeAvailable(espidflib)

set(EXTRA_COMPONENT_DIRS
    # $ENV{IDF_PATH}/examples/common_components/led_strip
    ${espidflib_SOURCE_DIR}/components
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

idf_build_get_property(IDF_VER IDF_VER)
message(STATUS "Detected IDF version ${IDF_VER}. Recommended version v5.3.1")
if(NOT IDF_VER MATCHES v5.3.1)
    message(WARNING "Detected IDF version ${IDF_VER} does not match recommended version v5.3.1")
endif()


option(APP_FORCE_TIME_RELOAD "Force setting of compile time to the connected RTC" OFF)

project(chicken-coop-esp)

# idf_build_set_property(CXX_COMPILE_OPTIONS "-std=c++17" APPEND)

set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 1)
set(APP_VERSION_REVISION 0)

configure_file(main/usr_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/usr_config.h)
