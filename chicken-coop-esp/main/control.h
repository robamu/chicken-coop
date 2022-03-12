#ifndef MAIN_CONTROL_H_
#define MAIN_CONTROL_H_

#include <driver/uart.h>

#include <array>
#include <ctime>

#include "i2cdev.h"
#include "motor.h"
#include "sdkconfig.h"

void controlTask(void* args);

class Controller {
 public:
  Controller(Motor& motor);
  void preTaskInit();

  static void taskEntryPoint(void* args);

  static int getDayMinutesFromHourAndMinute(int hour, int minute);

 private:
  static constexpr gpio_num_t I2C_SDA = static_cast<gpio_num_t>(CONFIG_I2C_SDA_PORT);
  static constexpr gpio_num_t I2C_SCL = static_cast<gpio_num_t>(CONFIG_I2C_SCL_PORT);
  static constexpr gpio_num_t DOOR_SWITCH_PORT =
      static_cast<gpio_num_t>(CONFIG_DOOR_SWITCH_STATE_PORT);

  static constexpr char CTRL_TAG[] = "ctrl";
  static constexpr char PATTERN_CHAR = 'C';
  static constexpr char CMD_MODE = 'C';
  static constexpr char CMD_MOTOR_CTRL = 'M';
  static constexpr char CMD_TIME = 'T';

  static constexpr char CMD_MODE_MANUAL = 'M';
  static constexpr char CMD_MODE_NORMAL = 'N';

  static constexpr char CMD_MOTOR_PROTECTED_MODE = 'P';
  static constexpr char CMD_MOTOR_FORCE_MODE = 'F';

  static constexpr char CMD_MOTOR_CTRL_OPEN = 'O';
  static constexpr char CMD_MOTOR_CTRL_CLOSE = 'C';

  enum class AppStates { INIT, IDLE, MANUAL } appState = AppStates::INIT;

  enum class DoorStates {
    UNKNOWN,
    DOOR_OPEN,
    DOOR_CLOSE,
  } doorState = DoorStates::UNKNOWN;

  enum class CmdStates { IDLE, MOTOR_CTRL_OPEN, MOTOR_CTRL_CLOSE } cmdState = CmdStates::IDLE;

  Motor& motor;
  TaskHandle_t taskHandle = nullptr;
  static constexpr uart_port_t UART_NUM = UART_NUM_1;
  static constexpr uint8_t UART_PATTERN_NUM = 2;
  static constexpr uint8_t UART_PATTERN_TIMEOUT = 5;
  static QueueHandle_t UART_QUEUE;
  static uart_config_t UART_CFG;
  std::array<uint8_t, 524> UART_RECV_BUF = {};
  static constexpr size_t UART_RING_BUF_SIZE = 524;
  static constexpr uint8_t UART_QUEUE_DEPTH = 20;
  i2c_dev_t i2c = {};

  tm currentTime = {};

  bool initPrintSwitch = true;
  bool openExecuted = false;
  bool closeExecuted = false;

  // Day from 0 to 30
  int currentDay = 0;
  // Month from 0 to 11
  int currentMonth = 0;

  int currentOpenDayMinutes = 0;
  int currentCloseDayMinutes = 0;

  void task();

  void stateMachine();
  void handleUartReception();
  void handleUartCommand(std::string cmd);
  // Returns 0 if initialization is done, otherwise 1
  int performInitMode();
  void updateDoorState();
  void performIdleMode();
  void updateCurrentOpenCloseTimes(bool printTimes);
  int initOpen();
  int initClose();
  /**
   * Call before starting the controller task!
   */
  static void uartInit();
};

struct ControllerArgs {
  Controller& controller;
};

#endif /* MAIN_CONTROL_H_ */
