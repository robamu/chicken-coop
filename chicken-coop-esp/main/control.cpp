#include "control.h"

#include <ds3231.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>

#include "compile_time.h"
#include "open_close_times.h"
#include "switch.h"
#include "usr_config.h"

static constexpr esp_log_level_t LOG_LEVEL = ESP_LOG_DEBUG;

QueueHandle_t Controller::UART_QUEUE = nullptr;
uart_config_t Controller::UART_CFG = {};

Controller::Controller() {}

void Controller::init() {
  esp_log_level_set(CTRL_TAG, LOG_LEVEL);
  uartInit();
}

void Controller::taskEntryPoint(void* args) {
  Controller ctrl;
  ctrl.task();
}

void Controller::task() {
  esp_task_wdt_add(nullptr);
  ESP_ERROR_CHECK(i2cdev_init());
  ESP_ERROR_CHECK(ds3231_init_desc(&i2c, I2C_NUM_0, I2C_SDA, I2C_SCL));
  while (true) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());
    // Handle all events
    handleUartReception();
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
    } else if (appState == AppStates::IDLE) {
      performIdleMode();
    }
    vTaskDelay(400);
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

void Controller::handleUartCommand(std::string cmd) {
  const char* rawCmd = cmd.data();
  if (cmd.length() == 2) {
    ESP_LOGI(CTRL_TAG, "Ping detected");
    // TODO: Send ping reply here
    return;
  }
  char cmdByte = rawCmd[2];
  switch (cmdByte) {
    case ('C'): {
      if (cmd.length() < 4) {
        ESP_LOGW(CTRL_TAG, "Invalid mode command detected");
        return;
      }
      char modeByte = rawCmd[3];
      if (modeByte == 'M') {
        // Switch to manual control
        ESP_LOGI(CTRL_TAG, "Switching to manual mode");
        appState = AppStates::MANUAL;
      } else if (modeByte == 'N') {
        ESP_LOGI(CTRL_TAG, "Switching to normal mode");
        appState = AppStates::INIT;
      } else {
        ESP_LOGW(CTRL_TAG, "Invalid mode specifier %c detected, M (manual) and N (normal) allowed",
                 modeByte);
      }
    }
  }
}

void Controller::uartInit() {
  UART_CFG.baud_rate = 115200;
  UART_CFG.data_bits = UART_DATA_8_BITS;
  UART_CFG.parity = UART_PARITY_DISABLE;
  UART_CFG.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  UART_CFG.stop_bits = UART_STOP_BITS_1;
  ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_RING_BUF_SIZE, UART_RING_BUF_SIZE,
                                      UART_QUEUE_DEPTH, &UART_QUEUE, 0));
  ESP_ERROR_CHECK(uart_param_config(UART_NUM, &UART_CFG));
  ESP_ERROR_CHECK(uart_set_pin(UART_NUM, CONFIG_COM_UART_TX, CONFIG_COM_UART_RX, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(UART_NUM, PATTERN_CHAR, UART_PATTERN_NUM,
                                                    UART_PATTERN_TIMEOUT, 0, 0));
  ESP_ERROR_CHECK(uart_pattern_queue_reset(UART_NUM, UART_QUEUE_DEPTH));
}

void Controller::handleUartReception() {
  uart_event_t event;
  while (xQueueReceive(UART_QUEUE, reinterpret_cast<void*>(&event), 0)) {
    switch (event.type) {
      case (UART_DATA): {
        break;
      }
      case (UART_PATTERN_DET): {
        size_t len = 0;
        uart_get_buffered_data_len(UART_NUM, &len);
        int pos = uart_pattern_pop_pos(UART_NUM);
        int nextPos = uart_pattern_get_pos(UART_NUM);
        ESP_LOGD(CTRL_TAG, "Detected UART pattern");
        ESP_LOGD(CTRL_TAG, "Total buffered length %d, position: %d, next position: %d", len, pos,
                 nextPos);
        if (pos > 0) {
          // Delete preceding data first
          uart_read_bytes(UART_NUM, UART_RECV_BUF.data(), pos, 0);
        }

        size_t cmdLen = 0;
        if (nextPos != -1 and nextPos > pos) {
          // Next block of data detected. Only read the relevant block
          cmdLen = nextPos - pos;
        } else {
          cmdLen = len - pos;
        }
        uart_read_bytes(UART_NUM, UART_RECV_BUF.data(), cmdLen, 0);
        // Last character
        if (UART_RECV_BUF[cmdLen - 1] != '\n') {
          ESP_LOGW(CTRL_TAG, "Invalid UART command, did not end with newline character");
          break;
        }
        UART_RECV_BUF[cmdLen - 1] = '\0';
        ESP_LOGI(CTRL_TAG, "Received command %s", UART_RECV_BUF.data());
        handleUartCommand(std::string(reinterpret_cast<const char*>(UART_RECV_BUF.data()), cmdLen));
        break;
      }
      default: {
        ESP_LOGW(CTRL_TAG, "Unknown event type");
      }
    }
  }
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
