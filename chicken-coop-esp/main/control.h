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
  enum class AppStates { START_DELAY, INIT, NORMAL, MANUAL };

  Controller(Led& led, AppStates initState = AppStates::START_DELAY);

  void setAppState(AppStates appState);
  void preTaskInit();

  static void taskEntryPoint(void* args);

  bool checkMotorOperationDone();

  static int getDayMinutesFromHourAndMinute(int hour, int minute);

 private:
  static constexpr gpio_num_t I2C_SDA = static_cast<gpio_num_t>(CONFIG_I2C_SDA_PORT);
  static constexpr gpio_num_t I2C_SCL = static_cast<gpio_num_t>(CONFIG_I2C_SCL_PORT);
  static constexpr gpio_num_t DOOR_SWITCH_PORT =
      static_cast<gpio_num_t>(CONFIG_DOOR_SWITCH_STATE_PORT);

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

  static constexpr uint32_t BLINK_PERIOD_NORMAL = 6000;
  static constexpr uint32_t BLINK_PERIOD_INIT = 3000;
  static constexpr uint32_t BLINK_PERIOD_MANUAL = 1000;

  enum class DoorStates {
    UNKNOWN,
    DOOR_OPEN,
    DOOR_CLOSE,
  };

  enum class MotorDriveState {
    IDLE,
    OPENING,
    CLOSING,
  } motorState = MotorDriveState::IDLE;

  Led& led;

  struct AppParams {
    bool openExecutedForTheDay = false;
    bool closeExecutedForTheDay = false;
  } appParams;

  enum class RecheckState {
    IDLE,
    ARMED,
    RETRYING,
    RECHECKING,
  };

  struct RecheckParameter {
    RecheckState recheckMode = RecheckState::ARMED;
    uint32_t recheckStartTimeTicks = 0;
    // Not used currently.
    uint32_t recheckCounter = 0;
  } recheckParams;

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

  TickType_t startTime = 0;
  TickType_t motorStartTime = 0;
  i2c_dev_t i2c = {};
  LedCfg motorOpCfg;
  LedCfg normalCfg;
  LedCfg initCfg;
  LedCfg manualCfg;

  char timeBuf[64] = {};

  tm currentTime = {};

  bool initPrintSwitch = true;
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
  // This is run after the controller has booted. It checks whether any operations are necessary.
  // Returns 0 if initialization is done, otherwise 1.
  int stateMachineInit();
  // This is the regular normal mode after the init mode has completed.
  void stateMachineNormal();

  void updateCurrentDayAndMonth();
  void updateCurrentOpenCloseTimes(bool printTimes);
  int initOpen();
  int initClose();
  void motorCtrlDone();
  void initCloseDoor();

  void openDoor();
  void closeDoor();
  void driveDoorMotor(bool dir1);
  void checkRecheckMechanism();

  /**
   * Call before starting the controller task!
   */
  static void uartInit();
};

struct ControllerArgs {
  Controller& controller;
};

#endif /* MAIN_CONTROL_H_ */
