#ifndef MAIN_MAIN_H_
#define MAIN_MAIN_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Handles for the tasks created by main(). */
extern TaskHandle_t CONTROL_TASK_HANDLE;
extern TaskHandle_t MOTOR_TASK_HANDLE;
extern TaskHandle_t LED_TASK_HANDLE;

#endif /* MAIN_MAIN_H_ */
