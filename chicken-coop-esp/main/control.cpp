#include "control.h"

#include "compile_time.h"
#include "ds3231.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "open_close_times.h"
#include "usr_config.h"

Controller::Controller() {}

void Controller::taskEntryPoint(void* args) {
  Controller ctrl;
  ctrl.taskLoop();
}

void Controller::taskLoop() {
  while (true) {
    i2cdev_init();
    esp_err_t result = ds3231_init_desc(&i2c, I2C_NUM_0, I2C_SDA, I2C_SCL);
    if (result != ESP_OK) {
      // print error
    }

#if APP_FORCE_TIME_RELOAD == 1
    // Set compile time
    time_t seconds = __TIME_UNIX__;
    tm* time = localtime(&seconds);
    ds3231_set_time(&i2c, time);
#endif

    ds3231_get_time(&i2c, &currentTime);
    // TODO:
    // 1. Read Clock time
    // 2. Check whether a new day has started
    // 3. If new day has started reset two
    if (appState == AppStates::INIT) {
      // TODO: Initialization or reboot, door state unknown. Get it from switch
      // ...
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
    vTaskDelay(20000);
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
