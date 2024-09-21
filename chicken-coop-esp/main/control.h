#ifndef MAIN_CONTROL_H_
#define MAIN_CONTROL_H_

#include <driver/uart.h>

#include <array>
#include <ctime>
#include <string>

#include "conf.h"
#include "i2cdev.h"
#include "led.h"
#include "motor.h"

void controlTask(void* args);

class Controller {
 public:
  enum class AppStates { START_DELAY, INIT, IDLE, MANUAL };

  Controller(Led& led, AppStates initState = AppStates::START_DELAY);

  void setAppState(AppStates appState);
  void preTaskInit();

  static void taskEntryPoint(void* args);

  static bool motorStopCondition(Direction dir, void* args);
  static Direction dirMapper(bool close);

  static int getDayMinutesFromHourAndMinute(int hour, int minute);

 private:
  static constexpr gpio_num_t I2C_SDA = static_cast<gpio_num_t>(CONFIG_I2C_SDA_PORT);
  static constexpr gpio_num_t I2C_SCL = static_cast<gpio_num_t>(CONFIG_I2C_SCL_PORT);
  static constexpr gpio_num_t DOOR_SWITCH_PORT =
      static_cast<gpio_num_t>(CONFIG_DOOR_SWITCH_STATE_PORT);

  static constexpr uint32_t START_DELAY_MS = 3000;

  static constexpr char CTRL_TAG[] = "ctrl";
  static constexpr char PATTERN_CHAR = 'C';

  enum class Cmds : char {
    MODE = 'C',
    MOTOR_CTRL = 'M',
    TIME = 'T',
    REQUEST = 'R',
  };

  enum class RequestCmds : char { TIME = 'T' };

  static constexpr char CMD_MODE_MANUAL = 'M';
  static constexpr char CMD_MODE_NORMAL = 'N';

  static constexpr char CMD_MOTOR_PROTECTED_MODE = 'P';
  static constexpr char CMD_MOTOR_FORCE_MODE = 'F';

  static constexpr char CMD_MOTOR_CTRL_OPEN = 'O';
  static constexpr char CMD_MOTOR_CTRL_CLOSE = 'C';
  static constexpr char CMD_MOTOR_CTRL_STOP = 'S';

  static constexpr uint32_t BLINK_PERIOD_IDLE = 5000;
  static constexpr uint32_t BLINK_PERIOD_INIT = 3000;

  enum class DoorStates {
    UNKNOWN,
    DOOR_OPEN,
    DOOR_CLOSE,
  } doorState = DoorStates::UNKNOWN;

  enum class MotorDriveState { IDLE, OPENING, CLOSING } motorState = MotorDriveState::IDLE;

  Led& led;
  AppStates appState = AppStates::INIT;
  TaskHandle_t taskHandle = nullptr;
  static constexpr uart_port_t UART_NUM = UART_NUM_1;
  static constexpr uint8_t UART_PATTERN_NUM = 2;
  static constexpr uint8_t UART_PATTERN_TIMEOUT = 5;
  static QueueHandle_t UART_QUEUE;
  static uart_config_t UART_CFG;
  std::array<uint8_t, 524> UART_RECV_BUF = {};
  std::array<uint8_t, 256> UART_REPLY_BUF = {};
  static constexpr size_t UART_RING_BUF_SIZE = 524;
  static constexpr uint8_t UART_QUEUE_DEPTH = 20;
  uint32_t startTime = 0;
  i2c_dev_t i2c = {};
  LedCfg motorOpCfg;
  LedCfg idleCfg;
  LedCfg initCfg;
  char timeBuf[64] = {};

  tm currentTime = {};

  bool initPrintSwitch = true;
  bool openExecuted = false;
  bool closeExecuted = false;
  bool forcedOp = false;

  // Day from 0 to 30
  int currentDay = -1;
  // Month from 0 to 11
  int currentMonth = -1;

  int currentOpenDayMinutes = 0;
  int currentCloseDayMinutes = 0;

  void task();

  bool validCmd(char rawCmd);
  void stateMachine();
  // Can be used if time is changed externally to re-trigger any door operations immediately
  void resetToInitState();
  void handleUartReception();
  void handleUartCommand(std::string cmd);
  // Returns 0 if initialization is done, otherwise 1
  int performInitMode();
  void updateDoorState();
  void performIdleMode();
  void updateCurrentDayAndMonth();
  void updateCurrentOpenCloseTimes(bool printTimes);
  int initOpen();
  int initClose();
  void motorCtrlDone();

  void openDoor();
  void closeDoor();
  void driveDoorMotor(bool dir1);

  /**
   * Call before starting the controller task!
   */
  static void uartInit();
};

struct ControllerArgs {
  Controller& controller;
};

#endif /* MAIN_CONTROL_H_ */
