#!/usr/bin/env python3
import RPi.GPIO as GPIO
import time

IN1 = 17
IN2 = 27
IN3 = 22
IN4 = 23

# careful lowering this, at some point you run into the mechanical limitation of how quick
# your motor can move
STEP_SLEEP = 0.002

STEP_COUNT = 4096  # 5.625*(1/64) per step, 4096 steps is 360°

direction = True  # True for clockwise, False for counter-clockwise

# defining stepper motor sequence (found in
# documentation http://www.4tronix.co.uk/arduino/Stepper-Motors.php)
step_sequence = [[1, 0, 0, 0],
                 [1, 0, 1, 0],
                 [0, 0, 1, 0],
                 [0, 1, 1, 0],
                 [0, 1, 0, 0],
                 [0, 1, 0, 1],
                 [0, 0, 0, 1],
                 [1, 0, 0, 1]]
motor_pins = [in1, in3, in2, in4]


def main():
    print("-- Chicken Coop Control --")
    print("INx pin to BCM mapping:")
    print(f"IN1 -> {in1} | IN2 -> {in2} | IN3 -> {in3} | IN4 -> {in4}")
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
                for pin in range(0, len(motor_pins)):
                    GPIO.output(motor_pins[pin], step_sequence[motor_step_counter][pin])
                if direction:
                    motor_step_counter = (motor_step_counter - 1) % 8
                elif not direction:
                    motor_step_counter = (motor_step_counter + 1) % 8
                else:  # defensive programming
                    print("uh oh... direction should *always* be either True or False")
                    cleanup()
                    exit(1)
                time.sleep(STEP_SLEEP)
        except KeyboardInterrupt:
            cleanup()
            exit(1)

    cleanup()
    exit(0)


def cleanup():
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.LOW)
    GPIO.cleanup()


if __name__ == "__main__":
    main()
