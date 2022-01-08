#!/usr/bin/env python3
import RPi.GPIO as GPIO
import time

IN1 = 17
IN2 = 27
IN3 = 22
IN4 = 23

# careful lowering this, at some point you run into the mechanical limitation of how quick
# your motor can move. 0.005 is about the found minimum
STEP_SLEEP = 0.005

STEP_COUNT = 4096  # 5.625*(1/64) per step, 4096 steps is 360°

# True for clockwise, False for counter-clockwise (when looking in motor direction)
DIRECTION = False

# defining stepper motor sequence (found in
# documentation http://www.4tronix.co.uk/arduino/Stepper-Motors.php)
STEP_SEQUENCE = [[1, 0, 0, 0],
                 [1, 0, 1, 0],
                 [0, 0, 1, 0],
                 [0, 1, 1, 0],
                 [0, 1, 0, 0],
                 [0, 1, 0, 1],
                 [0, 0, 0, 1],
                 [1, 0, 0, 1]]
MOTOR_PINS = [IN1, IN3, IN2, IN4]


def main():
    print("-- Chicken Coop Control --")
    print("INx pin to BCM mapping: ", end="")
    print(f"IN1 -> {IN1} | IN2 -> {IN2} | IN3 -> {IN3} | IN4 -> {IN4}")
    dir_str = "Clockwise" if DIRECTION else "Counter-Clockwise"
    print(f"Direction: {dir_str}")
    motor_step_counter = 0
    # setting up
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(IN1, GPIO.OUT)
    GPIO.setup(IN2, GPIO.OUT)
    GPIO.setup(IN3, GPIO.OUT)
    GPIO.setup(IN4, GPIO.OUT)

    # initializing
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.LOW)

    while True:
        # the meat
        try:
            for i in range(STEP_COUNT):
                for pin in range(0, len(MOTOR_PINS)):
                    GPIO.output(MOTOR_PINS[pin], STEP_SEQUENCE[motor_step_counter][pin])
                if DIRECTION:
                    motor_step_counter = (motor_step_counter - 1) % 8
                elif not DIRECTION:
                    motor_step_counter = (motor_step_counter + 1) % 8
                else:  # defensive programming
                    print("Error: Invalid direction")
                    cleanup()
                    exit(1)
                time.sleep(STEP_SLEEP)
        except KeyboardInterrupt:
            cleanup()
            exit(1)


def cleanup():
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.LOW)
    GPIO.cleanup()


if __name__ == "__main__":
    main()
