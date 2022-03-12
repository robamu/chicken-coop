#include "control.h"

#include <ds3231.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cinttypes>
#include <ctime>
#include <string>

#include "compile_time.h"
#include "open_close_times.h"
#include "switch.h"
#include "usr_config.h"

static constexpr esp_log_level_t LOG_LEVEL = ESP_LOG_DEBUG;

QueueHandle_t Controller::UART_QUEUE = nullptr;
uart_config_t Controller::UART_CFG = {};

Controller::Controller(Motor& motor) : motor(motor) {}

void Controller::preTaskInit() {
  esp_log_level_set(CTRL_TAG, LOG_LEVEL);
  uartInit();
}

void Controller::taskEntryPoint(void* args) {
  ControllerArgs* ctrlArgs = reinterpret_cast<ControllerArgs*>(args);
  if (args != nullptr) {
    ctrlArgs->controller.taskHandle = xTaskGetCurrentTaskHandle();
    ctrlArgs->controller.task();
  }
}

void Controller::task() {
  esp_task_wdt_add(nullptr);
  ESP_ERROR_CHECK(i2cdev_init());
  ESP_ERROR_CHECK(ds3231_init_desc(&i2c, I2C_NUM_0, I2C_SDA, I2C_SCL));
  while (true) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());
    stateMachine();
    vTaskDelay(400);
  }
}

void Controller::stateMachine() {
  // Handle all events
  handleUartReception();
#if APP_FORCE_TIME_RELOAD == 1
  // Set compile time
  time_t seconds = __TIME_UNIX__;
  tm* time = localtime(&seconds);
  ds3231_set_time(&i2c, time);
#endif

  ds3231_get_time(&i2c, &currentTime);
  updateDoorState();

  // INIT mode: System just came up and we need to check whether any operations are necessary
  // for the current time
  if (appState == AppStates::INIT) {
    if (initPrintSwitch) {
      ESP_LOGI(CTRL_TAG, "Entered initialization mode");
      char timeBuf[64] = {};
      strftime(timeBuf, sizeof(timeBuf) - 1, "%Y-%m-%d %H:%M:%S", &currentTime);
      ESP_LOGI(CTRL_TAG, "Detected current time: %s", timeBuf);
      if (doorswitch::opened()) {
        ESP_LOGI(CTRL_TAG, "Door is opened");
      } else {
        ESP_LOGI(CTRL_TAG, "Door is closed");
      }
      initPrintSwitch = false;
    }
    int result = performInitMode();
    if (result == 0) {
      ESP_LOGI(CTRL_TAG, "Going to IDLE mode");
      // If everything is done
      appState = AppStates::IDLE;
    }
  }
  if (appState == AppStates::IDLE) {
    performIdleMode();
  }
  // In manual mode, monitor the manual motor control operations
  if (appState == AppStates::MANUAL) {
    ESP_LOGV(CTRL_TAG, "Command state Manual Mode: %d", static_cast<uint8_t>(cmdState));
    switch (cmdState) {
      case (CmdStates::MOTOR_CTRL_CLOSE): {
        ESP_LOGV(CTRL_TAG, "Checking motor close done");
        if (motor.operationDone()) {
          ESP_LOGI(CTRL_TAG, "Close operation in manual mode done");
          cmdState = CmdStates::IDLE;
        }
        break;
      }
      case (CmdStates::MOTOR_CTRL_OPEN): {
        ESP_LOGV(CTRL_TAG, "Checking motor open done");
        if (motor.operationDone()) {
          ESP_LOGI(CTRL_TAG, "Open operation in manual mode done");
          cmdState = CmdStates::IDLE;
        }
        break;
      }
      case (CmdStates::IDLE):
      default: {
        break;
      }
    }
  }
}

int Controller::performInitMode() {
  int result = 0;
  currentDay = currentTime.tm_mday;
  currentMonth = currentTime.tm_mon;
  currentOpenDayMinutes =
      getDayMinutesFromHourAndMinute(OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][0],
                                     OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][1]);
  currentCloseDayMinutes =
      getDayMinutesFromHourAndMinute(OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][2],
                                     OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][3]);

  // There are three cases to consider here:
  //  1. Controller started before opening time
  //  2. Controller started during open time
  //  3. Controller started during close time
  // In the first case, both open and close need to be executed for that day but the door has
  // to be closed.
  // In the second case, if the door is closed, it needs to be opened. Otherwise, only close
  // needs to be executed for that day
  // In the third case, the door is closed if it is open, otherwise nothing needs to be done
  int hour = currentTime.tm_hour;
  int minute = currentTime.tm_min;
  int dayMinutes = getDayMinutesFromHourAndMinute(hour, minute);

  if (dayMinutes < currentOpenDayMinutes - 10) {
    // Case 1. Close the door if not already done
    result = initClose();
    if (result == 1) {
      openExecuted = false;
      closeExecuted = false;
      // Both operations needs to be performed in the IDLE mode, INIT mode done
      ESP_LOGI(CTRL_TAG, "Door needs to be both opened and closed for this day");
    }
    return result;
  } else if (dayMinutes >= currentOpenDayMinutes and dayMinutes < currentCloseDayMinutes - 10) {
    // Case 2
    result = initOpen();
    if (result == 1) {
      openExecuted = true;
      closeExecuted = false;
    }
    return result;
  } else if (dayMinutes >= currentCloseDayMinutes) {
    // Case 3
    result = initClose();
    if (result == 1) {
      openExecuted = true;
      closeExecuted = true;
    }
  }
  // For boundary cases, we are not done for now
  return 1;
}

void Controller::performIdleMode() {
  int monthIdx = currentTime.tm_mon;
  if (monthIdx > currentMonth) {
    currentMonth = monthIdx;
  }

  int day = currentTime.tm_mday;
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

  int hour = currentTime.tm_hour;
  int minute = currentTime.tm_min;
  int dayMinutes = getDayMinutesFromHourAndMinute(hour, minute);
  if (not openExecuted and dayMinutes >= currentOpenDayMinutes) {
    if (doorState == DoorStates::DOOR_CLOSE) {
      // Motor control might already be pending
      if (cmdState != CmdStates::MOTOR_CTRL_OPEN) {
        ESP_LOGI(CTRL_TAG, "Opening door in IDLE mode");
        bool success = motor.requestOpen();
        if (success) {
          cmdState = CmdStates::MOTOR_CTRL_OPEN;
        } else {
          ESP_LOGW(CTRL_TAG, "Tried to request door open in IDLE mode, but motor is busy");
        }
      }
    }
    if (cmdState == CmdStates::MOTOR_CTRL_OPEN) {
      if (motor.operationDone()) {
        ESP_LOGI(CTRL_TAG, "Door opening operation in IDLE mode done");
        cmdState = CmdStates::IDLE;
        openExecuted = true;
      }
    }
  }

  if (not closeExecuted and dayMinutes >= currentCloseDayMinutes) {
    if (doorState == DoorStates::DOOR_OPEN) {
      // Motor control might already be pending
      if (cmdState != CmdStates::MOTOR_CTRL_CLOSE) {
        ESP_LOGI(CTRL_TAG, "Closing door in IDLE mode");
        bool success = motor.requestClose();
        if (success) {
          cmdState = CmdStates::MOTOR_CTRL_CLOSE;
        } else {
          ESP_LOGW(CTRL_TAG, "Tried to request door close, but motor is busy");
        }
      }
    }
    if (cmdState == CmdStates::MOTOR_CTRL_CLOSE) {
      if (motor.operationDone()) {
        ESP_LOGI(CTRL_TAG, "Door closing operation in IDLE mode done");
        cmdState = CmdStates::IDLE;
        closeExecuted = true;
      }
    }
  }
}

int Controller::initOpen() {
  // This needs to be executed in any case
  if (doorState == DoorStates::DOOR_CLOSE) {
    if (cmdState == CmdStates::IDLE) {
      ESP_LOGI(CTRL_TAG, "Door needs to be opened in INIT mode. Opening door");
      bool requestSuccess = motor.requestOpen();
      if (requestSuccess) {
        cmdState = CmdStates::MOTOR_CTRL_OPEN;
      } else {
        ESP_LOGW(CTRL_TAG, "Tried to request door open in INIT mode, but motor is busy");
      }
    }
    if (cmdState == CmdStates::MOTOR_CTRL_OPEN) {
      if (motor.operationDone()) {
        ESP_LOGI(CTRL_TAG, "Door was opened in INIT mode");
        doorState = DoorStates::DOOR_OPEN;
        cmdState = CmdStates::IDLE;
      }
    }
  }
  if (doorState == DoorStates::DOOR_OPEN) {
    // All ok
    return 0;
  }
  return 1;
}

int Controller::initClose() {
  if (doorState == DoorStates::DOOR_OPEN) {
    if (cmdState == CmdStates::IDLE) {
      ESP_LOGI(CTRL_TAG, "Door needs to be closed in INIT mode. Closing door");
      bool requestSuccess = motor.requestClose();
      if (requestSuccess) {
        cmdState = CmdStates::MOTOR_CTRL_CLOSE;
      } else {
        ESP_LOGW(CTRL_TAG, "Tried to request door close in INIT mode, but motor is busy");
      }
    }
    if (cmdState == CmdStates::MOTOR_CTRL_CLOSE) {
      if (motor.operationDone()) {
        ESP_LOGI(CTRL_TAG, "Door was closed in INIT mode");
        doorState = DoorStates::DOOR_CLOSE;
        cmdState = CmdStates::IDLE;
      }
    }
  }
  if (doorState == DoorStates::DOOR_CLOSE) {
    // All ok
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
  if (cmd.length() == 3) {
    ESP_LOGI(CTRL_TAG, "Ping detected");
    // TODO: Send ping reply here
    return;
  }
  char cmdByte = rawCmd[2];
  switch (cmdByte) {
    case (CMD_MODE): {
      if (cmd.length() < 4) {
        ESP_LOGW(CTRL_TAG, "Invalid mode command detected");
        return;
      }
      char modeByte = rawCmd[3];
      if (modeByte == CMD_MODE_MANUAL) {
        // Switch to manual control
        ESP_LOGI(CTRL_TAG, "Switching to manual mode");
        if (cmdState != CmdStates::IDLE) {
          ESP_LOGW(CTRL_TAG, "Can not switch to manual mode while door operation is pending");
          return;
        } else {
          appState = AppStates::MANUAL;
        }
      } else if (modeByte == CMD_MODE_NORMAL) {
        ESP_LOGI(CTRL_TAG, "Switching to normal mode");
        appState = AppStates::INIT;
      } else {
        ESP_LOGW(CTRL_TAG, "Invalid mode specifier %c detected, M (manual) and N (normal) allowed",
                 modeByte);
      }
      break;
    }
    case (CMD_TIME): {
      std::string timeString = cmd.substr(3, cmd.size() - 3 - 1);
      ESP_LOGI(CTRL_TAG, "Received time string %s", timeString.c_str());
      struct tm timeParsed = {};
      char* parseResult = strptime(timeString.c_str(), "%Y-%m-%dT%H:%M:%SZ", &timeParsed);
      if (parseResult != nullptr) {
        ESP_LOGI(CTRL_TAG, "Setting received time in DS3231 clock");
        ds3231_set_time(&i2c, &timeParsed);
      } else {
        // Invalid date format. Send NAK reply
        ESP_LOGW(CTRL_TAG, "Invalid date format. Pointer where parsing failed: %d", parseResult);
      }
      break;
    }
    case (CMD_MOTOR_CTRL): {
      if (appState != AppStates::MANUAL) {
        ESP_LOGW(CTRL_TAG,
                 "Received motor control command but not in manual mode. "
                 "Activate manual mode first");
        return;
      }
      if (cmd.length() < 5) {
        ESP_LOGW(CTRL_TAG, "Invalid motor control command detected");
        return;
      }
      char protChar = rawCmd[3];
      bool protOn = true;
      if (protChar == CMD_MOTOR_FORCE_MODE) {
        protOn = false;
      }
      char dirChar = rawCmd[4];
      if (dirChar == CMD_MOTOR_CTRL_OPEN) {
        if (protOn and doorState == DoorStates::DOOR_OPEN) {
          ESP_LOGW(CTRL_TAG, "Door opening was requested but the door is already open");
          return;
        }
        ESP_LOGI(CTRL_TAG, "Opening door in manual mode");
        bool requestSuccess = motor.requestOpen();
        if (requestSuccess) {
          cmdState = CmdStates::MOTOR_CTRL_OPEN;
        } else {
          ESP_LOGW(CTRL_TAG, "Tried to request door open in MANUAL mode, but motor is busy");
        }
      } else if (dirChar == CMD_MOTOR_CTRL_CLOSE) {
        if (protOn and doorState == DoorStates::DOOR_CLOSE) {
          ESP_LOGW(CTRL_TAG, "Door closing was requested but the door is already open");
          return;
        }
        bool requestSuccess = motor.requestClose();
        if (requestSuccess) {
          ESP_LOGI(CTRL_TAG, "Requested to close door in manual mode");
          cmdState = CmdStates::MOTOR_CTRL_CLOSE;
        } else {
          ESP_LOGW(CTRL_TAG, "Tried to request door close in MANUAL mode, but motor is busy");
        }
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

int Controller::getDayMinutesFromHourAndMinute(int hour, int minute) { return hour * 60 + minute; }
