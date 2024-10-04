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
#include "conf.h"
#include "open_close_times.h"
#include "switch.h"
#include "usr_config.h"

static constexpr esp_log_level_t LOG_LEVEL = ESP_LOG_INFO;

QueueHandle_t Controller::UART_QUEUE = nullptr;
uart_config_t Controller::UART_CFG = {};

Controller::Controller(Led& led, AppStates initState) : led(led), appState(initState) {
  motorOpCfg.brightness = 64;
  motorOpCfg.color = Colors::GREEN;
  motorOpCfg.periodMs = 200;

  normalCfg.brightness = 64;
  normalCfg.color = Colors::WHITE_DIM;
  normalCfg.periodMs = BLINK_PERIOD_NORMAL;

  initCfg.brightness = 64;
  initCfg.color = Colors::WHITE_DIM;
  initCfg.periodMs = BLINK_PERIOD_INIT;

  manualCfg.brightness = 64;
  manualCfg.color = Colors::YELLOW;
  manualCfg.periodMs = BLINK_PERIOD_MANUAL;
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
    ESP_LOGI(CTRL_TAG, "Waiting for %lu seconds before going into initialization mode..",
             config::START_DELAY_MS / 1000);
  }
  while (true) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_task_wdt_reset());
    stateMachine();
    vTaskDelay(pdMS_TO_TICKS(100));
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
    if (pdTICKS_TO_MS(xTaskGetTickCount() - startTime) > config::START_DELAY_MS) {
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
    int result = stateMachineInit();
    if (result == 0) {
      ESP_LOGI(CTRL_TAG, "Going to IDLE mode");
      led.setCurrentCfg(normalCfg);
      // Ensure consistent state, no matter what the FSM did.
      motorCtrlDone();
      // If everything is done
      appState = AppStates::NORMAL;
    }
  }
  if (appState == AppStates::NORMAL) {
    stateMachineNormal();
  }
  // In manual mode, monitor the manual motor control operations
  if (appState == AppStates::MANUAL) {
    if (initPrintSwitch) {
      initPrintSwitch = false;
    }
    ESP_LOGV(CTRL_TAG, "Command state Manual Mode: %d", static_cast<uint8_t>(motorState));
    switch (motorState) {
      case (MotorDriveState::OPENING): {
        openDoor();
        break;
      }
      case (MotorDriveState::CLOSING): {
        closeDoor();
        break;
      }
      case (MotorDriveState::IDLE): {
        motor::stop();
      }
      default: {
        break;
      }
    }
  }
}

int Controller::stateMachineInit() {
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
      appParams.openExecutedForTheDay = false;
      appParams.closeExecutedForTheDay = false;
      // Both operations needs to be performed in the IDLE mode, INIT mode done
      ESP_LOGI(CTRL_TAG, "Door needs to be both opened and closed for this day");
    }
    return result;
  } else if (dayMinutes >= currentOpenDayMinutes and dayMinutes < currentCloseDayMinutes) {
    // Case 2
    result = initOpen();
    if (result == 0) {
      appParams.openExecutedForTheDay = true;
      appParams.closeExecutedForTheDay = false;
    }
    return result;
  } else if (dayMinutes >= currentCloseDayMinutes) {
    // Case 3
    result = initClose();
    if (result == 0) {
      appParams.openExecutedForTheDay = true;
      appParams.closeExecutedForTheDay = true;
    }
    return result;
  }
  // For boundary cases, we are not done for now
  return 0;
}

void Controller::stateMachineNormal() {
  int monthIdx = currentTime.tm_mon;
  if (monthIdx != currentMonth) {
    currentMonth = monthIdx;
  }

  int day = currentTime.tm_mday - 1;
  // new day has started. Each day, open and close need to be executed once for now
  if (day != currentDay) {
    currentDay = day;
    appParams.openExecutedForTheDay = false;
    appParams.closeExecutedForTheDay = false;
    ESP_LOGI(CTRL_TAG, "New day has started. Assigning new opening and closing times");
    updateCurrentOpenCloseTimes(true);
  }

  int hour = currentTime.tm_hour;
  int minute = currentTime.tm_min;
  int dayMinutes = getDayMinutesFromHourAndMinute(hour, minute);
  if (not appParams.openExecutedForTheDay and dayMinutes >= currentOpenDayMinutes) {
    if (doorState == DoorStates::DOOR_CLOSE) {
      // Motor control might already be pending
      if (motorState != MotorDriveState::OPENING) {
        ESP_LOGI(CTRL_TAG, "Opening door in IDLE mode");
        led.setCurrentCfg(motorOpCfg);
        openDoor();
        motorState = MotorDriveState::OPENING;
      }
    }
    if (motorState == MotorDriveState::OPENING) {
      if (checkMotorOperationDone()) {
        ESP_LOGI(CTRL_TAG, "Door opening operation in NORMAL mode done");
        if (doorswitch::closed()) {
          ESP_LOGW(CTRL_TAG, "Door should be opened but is closed according to switch");
          motor::stop();
        }
        motorCtrlDone();
        appParams.openExecutedForTheDay = true;
      }
    }
  }

  if ((not appParams.closeExecutedForTheDay and dayMinutes >= currentCloseDayMinutes) or
      recheckParams.recheckMode == RecheckState::RETRYING) {
    if (doorState == DoorStates::DOOR_OPEN) {
      // Motor control might already be pending
      if (motorState != MotorDriveState::CLOSING) {
        ESP_LOGI(CTRL_TAG, "Closing door in NORMAL mode");
        initCloseDoor();
      }
    }
    if (motorState == MotorDriveState::CLOSING) {
      if (checkMotorOperationDone()) {
        ESP_LOGI(CTRL_TAG, "Door closing operation in NORMAL mode done");
        if (doorswitch::opened()) {
          ESP_LOGW(CTRL_TAG, "Door should be closed but is open according to switch");
        }
        motorCtrlDone();
        appParams.closeExecutedForTheDay = true;
      }
    }
  }

  if (recheckParams.recheckMode == RecheckState::RECHECKING) {
    if (pdTICKS_TO_MS(xTaskGetTickCount() - recheckParams.recheckStartTimeTicks) >= 2000) {
      if (!doorswitch::closed()) {
        ESP_LOGI(CTRL_TAG, "Closing Recheck: Door not closed, re-trying");
        recheckParams.recheckMode = RecheckState::RETRYING;
        initCloseDoor();
      } else {
        ESP_LOGI(CTRL_TAG, "Closing Recheck: Door is closed, OK");
        recheckParams.recheckMode = RecheckState::IDLE;
      }
    }
  }
  // The recheck timer is rearmed after the close duration
  if (recheckParams.recheckMode == RecheckState::IDLE &&
      pdTICKS_TO_MS(xTaskGetTickCount() - recheckParams.recheckStartTimeTicks) >
          config::MAX_CLOSE_DURATION * 2) {
    ESP_LOGI(CTRL_TAG, "Closing Recheck: Rearming");
    recheckParams.recheckMode = RecheckState::ARMED;
  }
}

void Controller::initCloseDoor() {
  led.setCurrentCfg(motorOpCfg);
  closeDoor();
  motorState = MotorDriveState::CLOSING;
}

int Controller::initOpen() {
  // This needs to be executed in any case
  if (doorState == DoorStates::DOOR_CLOSE) {
    if (motorState == MotorDriveState::IDLE) {
      ESP_LOGI(CTRL_TAG, "Door needs to be opened in INIT mode. Opening door");
      led.setCurrentCfg(motorOpCfg);
      openDoor();
      motorState = MotorDriveState::OPENING;
    }
    if (motorState == MotorDriveState::OPENING) {
      if (checkMotorOperationDone()) {
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
    if (motorState == MotorDriveState::IDLE) {
      ESP_LOGI(CTRL_TAG, "Door needs to be closed in INIT mode. Closing door");
      led.setCurrentCfg(motorOpCfg);
      closeDoor();
      motorState = MotorDriveState::CLOSING;
    }
    if (motorState == MotorDriveState::CLOSING) {
      if (checkMotorOperationDone()) {
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
        led.setCurrentCfg(manualCfg);
        if (motorState != MotorDriveState::IDLE) {
          ESP_LOGW(CTRL_TAG, "Can not switch to manual mode while door operation is pending");
          return;
        } else {
          appState = AppStates::MANUAL;
        }
      } else if (modeByte == CMD_MODE_NORMAL) {
        ESP_LOGI(CTRL_TAG, "Switching to normal mode");
        led.setCurrentCfg(initCfg);
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
        ESP_LOGW(CTRL_TAG, "Invalid date format. Pointer where parsing failed: %s", parseResult);
      }
      break;
    }
    case (Cmds::MOTOR_CTRL): {
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

      if (appState != AppStates::MANUAL and dirChar != CMD_MOTOR_CTRL_STOP) {
        ESP_LOGW(CTRL_TAG,
                 "Received motor control command but not in manual mode. "
                 "Activate manual mode first");
        return;
      }
      if (dirChar == CMD_MOTOR_CTRL_OPEN) {
        if (protOn and doorState == DoorStates::DOOR_OPEN) {
          ESP_LOGW(CTRL_TAG, "Door opening was requested but the door is already open");
          return;
        }
        if (not protOn) {
          forcedOp = true;
        }
        ESP_LOGI(CTRL_TAG, "Opening door in manual mode");
        openDoor();
        motorState = MotorDriveState::OPENING;
      } else if (dirChar == CMD_MOTOR_CTRL_CLOSE) {
        if (protOn and doorState == DoorStates::DOOR_CLOSE) {
          ESP_LOGW(CTRL_TAG, "Door closing was requested but the door is already open");
          return;
        }
        if (not protOn) {
          forcedOp = true;
        }
        closeDoor();
        ESP_LOGI(CTRL_TAG, "Closing door in manual mode");
        motorState = MotorDriveState::CLOSING;
      } else if (dirChar == CMD_MOTOR_CTRL_STOP) {
        ESP_LOGI(CTRL_TAG, "Stopping motor in manual mode");
        motor::stop();
        motorState = MotorDriveState::IDLE;
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
             "Date: %02d.%02d | Opening time : %02lu:%02lu | "
             "Closing time for today: %02lu:%02lu",
             currentDay + 1, currentMonth + 1, openHour, openMinute, closeHour, closeMinute);
  }
  currentOpenDayMinutes = getDayMinutesFromHourAndMinute(openHour, openMinute);
  currentCloseDayMinutes = getDayMinutesFromHourAndMinute(closeHour, closeMinute);
}

bool Controller::checkMotorOperationDone() {
  if (motorState == MotorDriveState::IDLE) {
    return true;
  }
  if (motorState == MotorDriveState::CLOSING) {
    if (pdTICKS_TO_MS(xTaskGetTickCount() - motorStartTime) >= config::MAX_CLOSE_DURATION) {
      return true;
    } else if (not forcedOp) {
      return doorswitch::closed();
    }
  }
  if (motorState == MotorDriveState::OPENING) {
    if (pdTICKS_TO_MS(xTaskGetTickCount() - motorStartTime) >= config::OPEN_DURATION_MS) {
      return true;
    }
  }
  return false;
}

void Controller::setAppState(AppStates appState) { this->appState = appState; }

void Controller::motorCtrlDone() {
  if (appState == AppStates::NORMAL) {
    led.setCurrentCfg(normalCfg);
  } else if (appState == AppStates::INIT) {
    led.setCurrentCfg(initCfg);
  } else {
    led.blinkDefault();
  }
  if (motorState == MotorDriveState::CLOSING) {
    if (recheckParams.recheckMode == RecheckState::ARMED) {
      ESP_LOGI(CTRL_TAG, "Door Recheck: Door closed, rechecking soon");
      recheckParams.recheckMode = RecheckState::RECHECKING;
      recheckParams.recheckStartTimeTicks = xTaskGetTickCount();
    }
    if (recheckParams.recheckMode == RecheckState::RETRYING) {
      recheckParams.recheckMode = RecheckState::IDLE;
    }
  }
  motor::stop();
  motorState = MotorDriveState::IDLE;
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
  // Don't know what this is good for.. It works without it I think.
  // ESP_ERROR_CHECK(uart_pattern_queue_reset(UART_NUM, UART_QUEUE_DEPTH));
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
  appParams.openExecutedForTheDay = false;
  appParams.closeExecutedForTheDay = false;
}

void Controller::openDoor() { driveDoorMotor(false); }
void Controller::closeDoor() { driveDoorMotor(true); }

void Controller::driveDoorMotor(bool dir1) {
  // Cache the start time if we go from and idle motor to an active motor.
  // Required for stop condition detection and to limit the total time the motor may be active.
  if (motorState == MotorDriveState::IDLE) {
    motorStartTime = xTaskGetTickCount();
  }
#if CONFIG_INVERT_MOTOR_DIRECTION
  motor::driveDir(!dir1);
#else
  motor::driveDir(dir1);
#endif
}
