#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Handles for the tasks created by main(). */
TaskHandle_t controlTaskHandle = nullptr;
TaskHandle_t motorTaskHandle = nullptr;
TaskHandle_t ledTaskHandle = nullptr;

#endif /* MAIN_MAIN_H_ */
