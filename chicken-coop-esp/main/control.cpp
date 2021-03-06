#include "control.h"

#include <ds3231.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cinttypes>
#include <cstring>
#include <ctime>
#include <string>

#include "compile_time.h"
#include "open_close_times.h"
#include "switch.h"
#include "usr_config.h"

static constexpr esp_log_level_t LOG_LEVEL = ESP_LOG_INFO;

QueueHandle_t Controller::UART_QUEUE = nullptr;
uart_config_t Controller::UART_CFG = {};

Controller::Controller(Motor& motor, Led& led, AppStates initState)
    : motor(motor), led(led), appState(initState) {
  motorOpCfg.brightness = 64;
  motorOpCfg.color = Colors::GREEN;
  motorOpCfg.periodMs = 200;
  idleCfg.brightness = 64;
  idleCfg.color = Colors::WHITE_DIM;
  idleCfg.periodMs = BLINK_PERIOD_IDLE;
  initCfg.brightness = 64;
  initCfg.color = Colors::WHITE_DIM;
  initCfg.periodMs = BLINK_PERIOD_INIT;
}

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
  ds3231_get_time(&i2c, &currentTime);
  strftime(timeBuf, sizeof(timeBuf) - 1, "%Y-%m-%d %H:%M:%S", &currentTime);
  ESP_LOGI(CTRL_TAG, "Detected current time: %s", timeBuf);
  updateDoorState();
  if (doorswitch::opened()) {
    ESP_LOGI(CTRL_TAG, "Door is opened");
  } else {
    ESP_LOGI(CTRL_TAG, "Door is closed");
  }
  startTime = xTaskGetTickCount();
  if (appState == AppStates::START_DELAY) {
    ESP_LOGI(CTRL_TAG, "Waiting for %d seconds before going into initialization mode..",
             START_DELAY_MS / 1000);
  }
  while (true) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());
    stateMachine();
    vTaskDelay(400);
  }
}

void Controller::stateMachine() {
#if APP_FORCE_TIME_RELOAD == 1
  // Set compile time
  time_t seconds = __TIME_UNIX__;
  tm* time = localtime(&seconds);
  ds3231_set_time(&i2c, time);
#endif

  ds3231_get_time(&i2c, &currentTime);

  // Handle all events
  handleUartReception();

  updateDoorState();

  // INIT mode: System just came up and we need to check whether any operations are necessary
  // for the current time
  if (appState == AppStates::START_DELAY) {
    if (xTaskGetTickCount() - startTime > START_DELAY_MS) {
      ESP_LOGI(CTRL_TAG, "Going into INIT mode");
      appState = AppStates::INIT;
    } else {
      return;
    }
  }
  if (appState == AppStates::INIT) {
    if (initPrintSwitch) {
      updateCurrentDayAndMonth();
      updateCurrentOpenCloseTimes(true);
      initPrintSwitch = false;
    }
    int result = performInitMode();
    if (result == 0) {
      ESP_LOGI(CTRL_TAG, "Going to IDLE mode");
      led.setCurrentCfg(idleCfg);
      // If everything is done
      appState = AppStates::IDLE;
    }
  }
  if (appState == AppStates::IDLE) {
    performIdleMode();
  }
  // In manual mode, monitor the manual motor control operations
  if (appState == AppStates::MANUAL) {
    if (initPrintSwitch) {
      initPrintSwitch = false;
    }
    ESP_LOGV(CTRL_TAG, "Command state Manual Mode: %d", static_cast<uint8_t>(cmdState));
    switch (cmdState) {
      case (CmdStates::MOTOR_CTRL_CLOSE): {
        ESP_LOGV(CTRL_TAG, "Checking motor close done");
        if (motor.operationDone()) {
          ESP_LOGI(CTRL_TAG, "Close operation in manual mode done");
          motorCtrlDone();
        }
        break;
      }
      case (CmdStates::MOTOR_CTRL_OPEN): {
        ESP_LOGV(CTRL_TAG, "Checking motor open done");
        if (motor.operationDone()) {
          ESP_LOGI(CTRL_TAG, "Open operation in manual mode done");
          motorCtrlDone();
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

  if (dayMinutes < currentOpenDayMinutes) {
    // Case 1. Close the door if not already done
    result = initClose();
    if (result == 0) {
      openExecuted = false;
      closeExecuted = false;
      // Both operations needs to be performed in the IDLE mode, INIT mode done
      ESP_LOGI(CTRL_TAG, "Door needs to be both opened and closed for this day");
    }
    return result;
  } else if (dayMinutes >= currentOpenDayMinutes and dayMinutes < currentCloseDayMinutes) {
    // Case 2
    result = initOpen();
    if (result == 0) {
      openExecuted = true;
      closeExecuted = false;
    }
    return result;
  } else if (dayMinutes >= currentCloseDayMinutes) {
    // Case 3
    result = initClose();
    if (result == 0) {
      openExecuted = true;
      closeExecuted = true;
    }
    return result;
  }
  // For boundary cases, we are not done for now
  return 0;
}

void Controller::performIdleMode() {
  int monthIdx = currentTime.tm_mon;
  if (monthIdx != currentMonth) {
    currentMonth = monthIdx;
  }

  int day = currentTime.tm_mday - 1;
  // new day has started. Each day, open and close need to be executed once for now
  if (day != currentDay) {
    currentDay = day;
    openExecuted = false;
    closeExecuted = false;
    ESP_LOGI(CTRL_TAG, "New day has started. Assigning new opening and closing times");
    updateCurrentOpenCloseTimes(true);
  }

  int hour = currentTime.tm_hour;
  int minute = currentTime.tm_min;
  int dayMinutes = getDayMinutesFromHourAndMinute(hour, minute);
  if (not openExecuted and dayMinutes >= currentOpenDayMinutes) {
    if (doorState == DoorStates::DOOR_CLOSE) {
      // Motor control might already be pending
      if (cmdState != CmdStates::MOTOR_CTRL_OPEN) {
        ESP_LOGI(CTRL_TAG, "Opening door in IDLE mode");
        led.setCurrentCfg(motorOpCfg);
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
        if (doorswitch::closed()) {
          ESP_LOGW(CTRL_TAG, "Door should be opened but is closed according to switch");
        }
        motorCtrlDone();
        openExecuted = true;
      }
    }
  }

  if (not closeExecuted and dayMinutes >= currentCloseDayMinutes) {
    if (doorState == DoorStates::DOOR_OPEN) {
      // Motor control might already be pending
      if (cmdState != CmdStates::MOTOR_CTRL_CLOSE) {
        ESP_LOGI(CTRL_TAG, "Closing door in IDLE mode");
        led.setCurrentCfg(motorOpCfg);
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
        if (doorswitch::opened()) {
          ESP_LOGW(CTRL_TAG, "Door should be closed but is open according to switch");
        }
        motorCtrlDone();
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
      led.setCurrentCfg(motorOpCfg);
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
        led.blinkDefault();
        motorCtrlDone();
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
      led.setCurrentCfg(motorOpCfg);
      ;
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
        if (doorswitch::opened()) {
          ESP_LOGW(CTRL_TAG, "Door should be closed but is opened according to switch");
        }
        doorState = DoorStates::DOOR_CLOSE;
        led.blinkDefault();
        motorCtrlDone();
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
    size_t currentIdx = 0;
    ESP_LOGI(CTRL_TAG, "Ping detected");
    UART_REPLY_BUF[currentIdx] = PATTERN_CHAR;
    currentIdx++;
    UART_REPLY_BUF[currentIdx] = PATTERN_CHAR;
    currentIdx++;
    UART_REPLY_BUF[currentIdx] = '\n';
    currentIdx++;
    int result = uart_write_bytes(UART_NUM, UART_REPLY_BUF.data(), currentIdx);
    if (result < 0) {
      ESP_LOGI(CTRL_TAG, "UART write failed with code: %d", result);
    }
    return;
  }
  char cmdByte = rawCmd[2];
  if (not validCmd(cmdByte)) {
    ESP_LOGW(CTRL_TAG, "Invalid command byte %c detected", cmdByte);
    return;
  }
  Cmds typedCmd = static_cast<Cmds>(cmdByte);
  switch (typedCmd) {
    case (Cmds::MODE): {
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
        resetToInitState();
      } else {
        ESP_LOGW(CTRL_TAG, "Invalid mode specifier %c detected, M (manual) and N (normal) allowed",
                 modeByte);
      }
      break;
    }
    case (Cmds::REQUEST): {
      if (cmd.length() < 4) {
        ESP_LOGW(CTRL_TAG, "Invalid print command detected");
        return;
      }
      char printChar = rawCmd[3];
      if (printChar == static_cast<char>(RequestCmds::TIME)) {
        size_t strLen = strftime(timeBuf, sizeof(timeBuf) - 1, "%Y-%m-%d %H:%M:%S", &currentTime);
        ESP_LOGI(CTRL_TAG, "Current time %s was reqeusted", timeBuf);
        size_t currentIdx = 0;
        UART_REPLY_BUF[currentIdx] = PATTERN_CHAR;
        currentIdx++;
        UART_REPLY_BUF[currentIdx] = PATTERN_CHAR;
        currentIdx++;
        UART_REPLY_BUF[currentIdx] = static_cast<char>(typedCmd);
        currentIdx++;
        UART_REPLY_BUF[currentIdx] = printChar;
        currentIdx++;
        if (strLen + 3 > UART_REPLY_BUF.size()) {
          // This should never happen..
          return;
        }
        std::memcpy(UART_REPLY_BUF.data() + currentIdx, timeBuf, strLen);
        currentIdx += strLen;
        UART_REPLY_BUF[currentIdx] = '\n';
        currentIdx += 1;
        int result = uart_write_bytes(UART_NUM, UART_REPLY_BUF.data(), currentIdx);
        if (result < 0) {
          ESP_LOGI(CTRL_TAG, "UART write failed with code: %d", result);
        }
      }
      break;
    }
    case (Cmds::TIME): {
      std::string timeString = cmd.substr(3, cmd.size() - 3 - 1);
      ESP_LOGI(CTRL_TAG, "Received time string %s", timeString.c_str());
      struct tm timeParsed = {};
      char* parseResult = strptime(timeString.c_str(), "%Y-%m-%dT%H:%M:%SZ", &timeParsed);
      if (parseResult != nullptr) {
        ESP_LOGI(CTRL_TAG, "Setting received time in DS3231 clock");
        ds3231_set_time(&i2c, &timeParsed);
        ESP_LOGI(CTRL_TAG, "Setting INIT mode");
        resetToInitState();
      } else {
        // Invalid date format. Send NAK reply
        ESP_LOGW(CTRL_TAG, "Invalid date format. Pointer where parsing failed: %d", parseResult);
      }
      break;
    }
    case (Cmds::MOTOR_CTRL): {
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
        if (not protOn) {
          forcedOp = true;
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
        if (not protOn) {
          forcedOp = true;
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

void Controller::updateCurrentOpenCloseTimes(bool printTimes) {
  if (currentMonth == -1 or currentDay == -1) {
    ESP_LOGE(CTRL_TAG, "Invalid current month or current day");
  }
  uint32_t openHour = OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][0];
  uint32_t openMinute = OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][1];
  uint32_t closeHour = OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][2];
  uint32_t closeMinute = OPEN_CLOSE_MONTHS[currentMonth]->month[currentDay][3];
  if (printTimes) {
    ESP_LOGI(CTRL_TAG,
             "Date: %02d.%02d | Opening time : %02d:%02d | "
             "Closing time for today: %02d:%02d",
             currentDay + 1, currentMonth + 1, openHour, openMinute, closeHour, closeMinute);
  }
  currentOpenDayMinutes = getDayMinutesFromHourAndMinute(openHour, openMinute);
  currentCloseDayMinutes = getDayMinutesFromHourAndMinute(closeHour, closeMinute);
}

bool Controller::motorStopCondition(Direction dir, uint8_t revIdx, void* args) {
  Controller* ctrl = reinterpret_cast<Controller*>(args);
  Direction openDir = dirMapper(false);
  Direction closeDir = dirMapper(true);
  bool result = false;
  if (dir == closeDir) {
    // Upper limit for closing
    if (revIdx == config::REVOLUTIONS_CLOSE_MAX) {
      result = true;
    } else {
      if (not ctrl->forcedOp) {
        // Default condition for closing
        result = doorswitch::closed();
      }
    }
  } else if (dir == openDir) {
    if (revIdx == config::REVOLUTIONS_OPEN_MAX) {
      result = true;
    }
  }
  return result;
}

Direction Controller::dirMapper(bool close) {
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

void Controller::setAppState(AppStates appState) { this->appState = appState; }

void Controller::motorCtrlDone() {
  if (appState == AppStates::IDLE) {
    led.setCurrentCfg(idleCfg);
  } else if (appState == AppStates::INIT) {
    led.setCurrentCfg(initCfg);
  } else {
    led.blinkDefault();
  }
  cmdState = CmdStates::IDLE;
  if (forcedOp) {
    forcedOp = false;
  }
}

bool Controller::validCmd(char rawCmd) {
  if (rawCmd == 'C' or rawCmd == 'T' or rawCmd == 'R' or rawCmd == 'M') {
    return true;
  }
  return false;
}

void Controller::updateCurrentDayAndMonth() {
  // See: https://www.cplusplus.com/reference/ctime/tm/
  // Month goes from 0 to 11, but day from 1 - 31
  currentDay = currentTime.tm_mday - 1;
  currentMonth = currentTime.tm_mon;
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

void Controller::resetToInitState() {
  appState = AppStates::INIT;
  openExecuted = false;
  closeExecuted = false;
}
