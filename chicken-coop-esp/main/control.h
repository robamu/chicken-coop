#ifndef MAIN_CONTROL_H_
#define MAIN_CONTROL_H_

#include <ctime>

#include "i2cdev.h"
#include "sdkconfig.h"

void controlTask(void* args);

class Controller {
 public:
  Controller();
  static void taskEntryPoint(void* args);

  void taskLoop();

  static int getDayMinutesFromHourAndMinute(int hour, int minute);

 private:
  enum class AppStates { INIT, IDLE };

  enum class DoorStates {
    UNKNOWN,
    DOOR_OPEN,
    DOOR_CLOSE,
  };

  enum class CmdStates { IDLE, MOTOR_CTRL_OPEN, MOTOR_CTRL_CLOSE };

  i2c_dev_t i2c;

  static constexpr gpio_num_t I2C_SDA = static_cast<gpio_num_t>(CONFIG_I2C_SDA_PORT);
  static constexpr gpio_num_t I2C_SCL = static_cast<gpio_num_t>(CONFIG_I2C_SCL_PORT);

  static constexpr gpio_num_t DOOR_SWITCH_PORT = static_cast<gpio_num_t>(CONFIG_DOOR_SWITCH_STATE_PORT);

  tm currentTime = {};
  // Internal state
  DoorStates doorState = DoorStates::UNKNOWN;
  CmdStates cmdState = CmdStates::IDLE;
  AppStates appState = AppStates::IDLE;

  bool openExecuted = false;
  bool closeExecuted = false;


  // Day from 0 to 30
  int currentDay = 0;
  // Month from 0 to 11
  int currentMonth = 0;

  int currentOpenDayMinutes = 0;
  int currentCloseDayMinutes = 0;

  // Returns 0 if initialization is done, otherwise 1
  int performInitMode();
  void performIdleMode();
  int initOpen();
  int initClose();
};

#endif /* MAIN_CONTROL_H_ */
