idf_component_register(SRCS 
    "main.cpp"
    "led.cpp"
    "motor.cpp"
    INCLUDE_DIRS "."
)

configure_file(usr_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config/usr_config.h)
