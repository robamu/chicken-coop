#ifndef MAIN_CONTROL_H_
#define MAIN_CONTROL_H_

#include <driver/uart.h>

#include <array>
#include <ctime>

#include "i2cdev.h"
#include "sdkconfig.h"

void controlTask(void* args);

class Controller {
 public:
  Controller();
  static void taskEntryPoint(void* args);
  void task();

  static int getDayMinutesFromHourAndMinute(int hour, int minute);

 private:
  static constexpr gpio_num_t I2C_SDA = static_cast<gpio_num_t>(CONFIG_I2C_SDA_PORT);
  static constexpr gpio_num_t I2C_SCL = static_cast<gpio_num_t>(CONFIG_I2C_SCL_PORT);
  static constexpr gpio_num_t DOOR_SWITCH_PORT =
      static_cast<gpio_num_t>(CONFIG_DOOR_SWITCH_STATE_PORT);

  static constexpr char CTRL_TAG[] = "ctrl";
  enum class AppStates { INIT, IDLE } appState = AppStates::IDLE;

  enum class DoorStates {
    UNKNOWN,
    DOOR_OPEN,
    DOOR_CLOSE,
  } doorState = DoorStates::UNKNOWN;

  enum class CmdStates { IDLE, MOTOR_CTRL_OPEN, MOTOR_CTRL_CLOSE } cmdState = CmdStates::IDLE;

  static constexpr uart_port_t UART_NUM = UART_NUM_1;
  static constexpr uint8_t UART_PATTERN_NUM = 2;
  QueueHandle_t uartQueue = nullptr;
  uart_config_t uartCfg = {};
  static constexpr size_t UART_RING_BUF_SIZE = 524;
  static constexpr uint8_t UART_QUEUE_DEPTH = 20;
  i2c_dev_t i2c = {};

  tm currentTime = {};

  bool openExecuted = false;
  bool closeExecuted = false;

  // Day from 0 to 30
  int currentDay = 0;
  // Month from 0 to 11
  int currentMonth = 0;

  int currentOpenDayMinutes = 0;
  int currentCloseDayMinutes = 0;

  void uartInit();
  // Returns 0 if initialization is done, otherwise 1
  int performInitMode();
  void updateDoorState();
  void performIdleMode();
  int initOpen();
  int initClose();
};

#endif /* MAIN_CONTROL_H_ */
