#include "control.h"

#include "compile_time.h"
#include "ds3231.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "open_close_times.h"
#include "switch.h"
#include "usr_config.h"

Controller::Controller() {}

void Controller::taskEntryPoint(void* args) {
  Controller ctrl;
  ctrl.task();
}

void Controller::task() {
  esp_task_wdt_add(nullptr);
  uartInit();
  ESP_ERROR_CHECK(i2cdev_init());
  ESP_ERROR_CHECK(ds3231_init_desc(&i2c, I2C_NUM_0, I2C_SDA, I2C_SCL));
  uart_event_t event;
  while (true) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());
    // Handle all events
    while (xQueueReceive(uartQueue, reinterpret_cast<void*>(&event), 0)) {
      switch (event.type) {
        case (UART_PATTERN_DET): {
          int pos = uart_pattern_pop_pos(UART_NUM);
          int nextPos = uart_pattern_pop_pos(UART_NUM);
          ESP_LOGI(CTRL_TAG, "Detected UART pattern. Position: %d, next position: %d", pos,
                   nextPos);
          break;
        }
        default: {
          ESP_LOGE(CTRL_TAG, "Unknown event type");
        }
      }
    }

#if APP_FORCE_TIME_RELOAD == 1
    // Set compile time
    time_t seconds = __TIME_UNIX__;
    tm* time = localtime(&seconds);
    ds3231_set_time(&i2c, time);
#endif

    ds3231_get_time(&i2c, &currentTime);
    // TODO:
    // 1. Check whether a new day has started
    // 2. If new day has started reset state
    if (appState == AppStates::INIT) {
      updateDoorState();
      int result = performInitMode();
      if (result == 0) {
        // If everything is done
        appState = AppStates::IDLE;
      }
    } else {
      performIdleMode();
    }
    // This would be the place to enter sleep mode to save power..
    // For now, delay for 20 seconds
    vTaskDelay(1000);
  }
}

int Controller::performInitMode() {
  // read day, month, hour and minute  from clock
  // ...
  currentDay = 0;
  currentMonth = 0;

  // Assign static variables
  currentOpenDayMinutes =
      getDayMinutesFromHourAndMinute(OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][0],
                                     OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][1]);
  currentCloseDayMinutes =
      getDayMinutesFromHourAndMinute(OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][2],
                                     OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][3]);

  // There are three cases to consider here
  // 1. Controller started before opening time
  // 2. Controller started during open time
  // 3. Controller started during close time
  // In the first case, both open and close need to be executed
  // In the second case, if the door is closed, it needs to be opened. Otherwise, only close
  // needs to be executed for that day
  // In the third case, the door is closed if it is open, otherwise nothing needs to be done
  int hour = 0;
  int minute = 0;
  int dayMinutes = getDayMinutesFromHourAndMinute(hour, minute);

  // Case 1
  if (dayMinutes < currentOpenDayMinutes - 10) {
    // Both operations needs to be performed
    openExecuted = false;
    closeExecuted = false;
    return 0;

    // Case 2
  } else if (dayMinutes >= currentOpenDayMinutes and dayMinutes < currentCloseDayMinutes - 10) {
    initOpen();
    // Case 3
  } else if (dayMinutes >= currentCloseDayMinutes) {
    initClose();
  }
  return 0;
}

void Controller::performIdleMode() {
  int monthIdx = 0;
  // read month somewhere from clock
  // ...
  if (monthIdx > currentMonth) {
    currentMonth = monthIdx;
  }

  int day = 0;
  // read day somewhere from clock
  // ...
  // new day has started. Each day, open and close need to be executed once for now
  if (day > currentDay) {
    currentDay = day;
    openExecuted = false;
    closeExecuted = false;
    // Assign static variables
    currentOpenDayMinutes =
        getDayMinutesFromHourAndMinute(OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][0],
                                       OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][1]);
    currentCloseDayMinutes =
        getDayMinutesFromHourAndMinute(OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][2],
                                       OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][3]);
  }

  int hour = 0;
  int minute = 0;
  // TODO: Read hour and minute somewhere from the clock
  // ...
  int dayMinutes = getDayMinutesFromHourAndMinute(hour, minute);
  if (not openExecuted and doorState == DoorStates::DOOR_CLOSE) {
    if (dayMinutes >= currentOpenDayMinutes) {
      // Motor control might already be pending
      if (cmdState != CmdStates::MOTOR_CTRL_OPEN) {
        // Send signal to open door
        // ...
        cmdState = CmdStates::MOTOR_CTRL_OPEN;
      } else {
        // Check whether opening has finished
        // ...
        // if it has finished
        cmdState = CmdStates::IDLE;
      }
    }
  }

  if (not closeExecuted and doorState == DoorStates::DOOR_OPEN) {
    if (dayMinutes >= closeExecuted) {
      // Motor control might already be pending
      if (cmdState == CmdStates::IDLE) {
        // Send signal to open door
        // ...
        cmdState = CmdStates::MOTOR_CTRL_CLOSE;
      } else if (cmdState == CmdStates::MOTOR_CTRL_CLOSE) {
        // Check whether opening has finished
        // ...
        // if it has finished
        cmdState = CmdStates::IDLE;
      } else {
        // Maybe blink LED red or something? This should not happen
      }
    }
  }
}

int Controller::initOpen() {
  // This needs to be executed in any case
  closeExecuted = false;
  // TODO: Find out if door is closed or open. Open it if it is not
  // ...
  if (doorState == DoorStates::DOOR_CLOSE) {
    if (cmdState == CmdStates::IDLE) {
      // TODO: Open door here
      cmdState = CmdStates::MOTOR_CTRL_OPEN;
    } else {
      // Door op might be pending . Check whether it is done
      // if it is done
      cmdState = CmdStates::IDLE;
      doorState = DoorStates::DOOR_OPEN;
      openExecuted = true;
      return 0;
    }
  } else {
    // All ok
    openExecuted = true;
    return 0;
  }
  return 1;
}

void Controller::updateDoorState() {
  if (doorswitch::opened()) {
    doorState = DoorStates::DOOR_OPEN;
  } else {
    doorState = DoorStates::DOOR_CLOSE;
  }
}

void Controller::uartInit() {
  uartCfg.baud_rate = 115200;
  uartCfg.data_bits = UART_DATA_8_BITS;
  uartCfg.parity = UART_PARITY_DISABLE;
  uartCfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uartCfg.stop_bits = UART_STOP_BITS_1;
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uartCfg));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, CONFIG_COM_UART_TX, CONFIG_COM_UART_RX, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_RING_BUF_SIZE, UART_RING_BUF_SIZE,
                                      UART_QUEUE_DEPTH, &uartQueue, 0));
  ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(UART_NUM, 'C', UART_PATTERN_NUM, 10, 10, 0));
  ESP_ERROR_CHECK(uart_pattern_queue_reset(UART_NUM, UART_QUEUE_DEPTH));
}

int Controller::initClose() {
  // Open Operation not necessary anymore
  openExecuted = true;
  // Find out if door is closed or open. Close it if it is not
  // ...
  if (doorState == DoorStates::DOOR_OPEN) {
    if (cmdState == CmdStates::IDLE) {
      // TODO: Close door
      cmdState = CmdStates::MOTOR_CTRL_CLOSE;
    } else {
      // TODO: Door op might be pending . Check whether it is done
      // ...
      // if it is done
      cmdState = CmdStates::IDLE;
      doorState = DoorStates::DOOR_CLOSE;
      closeExecuted = true;
    }
  } else {
    // All ok
    closeExecuted = true;
    return 0;
  }
  return 1;
}

int Controller::getDayMinutesFromHourAndMinute(int hour, int minute) { return hour * 60 + minute; }
