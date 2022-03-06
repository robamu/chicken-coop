import os
import serial
import enum
import configparser
from mod.ser import prompt_com_port, find_com_port_from_hint


class Languages(enum.IntEnum):
    ENGLISH = 0
    GERMAN = 1


INI_FILE = "config.ini"


class Config:
    com_port = ""
    com_port_hint = ""
    language = Languages.GERMAN


CFG = Config()


CMD_PATTERN = "CC"
CMD_TERMINATION = "\n"

CMD_MODE = "C"
CMD_MOTOR_CTRL = "M"
CMD_DATA = "D"
CMD_TIME = "T"

CMD_MODE_MANUAL = "M"
CMD_MODE_NORMAL = "N"

REQUEST_STR = [
    "Please specify a command. 'x' to exit, 'h' to display commands: ",
    "Bitte Kommando eingeben. 'x' um zu beenden, 'h' für Hilfe: ",
]

TERMINATION_PRINT = [
    "Closing Chicken Coop Door Client",
    "Schließe Hühnerklappen Client",
]
PING_PRINT = [
    "Sending ping to controller",
    "Sende Ping an den Controller"
]

PING_INDEX = 1
MAN_CTRL_IDX = 2
NORM_CTRL_IDX = 3
SET_TIME_IDX = 4
OPEN_IDX = 5
CLOSE_IDX = 6

MOTOR_MANUAL_STR = [
    "Sending command to switch to manual motor control",
    "Sende Kommando für manuelle Türkontrolle",
]
MOTOR_NORMAL_STR = [
    "Sending command to switch to nrormal motor control",
    "Sende Kommando für normale Türkontrolle",
]

def main():
    setup_cfg_from_ini()
    ser = serial.Serial(CFG.com_port, baudrate=115200)
    display_commands(CFG.language)
    while True:
        request_cmd = input(REQUEST_STR[CFG.language])
        if request_cmd.lower() in ["x"]:
            break
        if request_cmd.lower() in ["h"]:
            display_commands(CFG.language)
            continue
        if request_cmd.lower() in [str(MAN_CTRL_IDX)]:
            print(MOTOR_MANUAL_STR[CFG.language])
            cmd = CMD_PATTERN + CMD_MODE + CMD_MODE_MANUAL + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(PING_INDEX)]:
            print(PING_PRINT[CFG.language])
            cmd = CMD_PATTERN + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(NORM_CTRL_IDX)]:
            print(MOTOR_NORMAL_STR[CFG.language])
            cmd = CMD_PATTERN + CMD_MODE + CMD_MODE_NORMAL + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
    print(TERMINATION_PRINT[CFG.language])


def display_commands(language: Languages):
    print("-" * 40)
    if language == Languages.ENGLISH:
        print("Commands:")
        print(f"{PING_INDEX}: Send ping to ESP32 controller")
        print(f"{MAN_CTRL_IDX}: Switch to Manual Control Mode")
        print(f"{NORM_CTRL_IDX}: Switch to Normal Control Mode")
        print(f"{SET_TIME_IDX}: Updates the Time of the ESP32 controller")
        print(f"{OPEN_IDX}: Motor Control Open. Only works in manual mode")
        print(f"{CLOSE_IDX}: Motor Control Close. Only works in normal mode")
    elif language == Languages.GERMAN:
        print("Kommandos:")
        print(f"{PING_INDEX}: Ping zum ESP32 Controller senden")
        print(f"{MAN_CTRL_IDX}: Wechsel in den manuellen Kontrollmodus")
        print(f"{NORM_CTRL_IDX}: Wechsel in den normalen Kontrollmodus")
        print(f"{SET_TIME_IDX}: Aktualisiert die Zeit des ESP32 Controller")
        print(f"{OPEN_IDX}: Klappe auf. Funktioniert nur im manuellen Kontrollmodus")
        print(f"{CLOSE_IDX}: Klappe zu. Funktioniert nur im manuellen Kontrollmodus")


def setup_cfg_from_ini():
    com_port = None
    com_port_hint = None
    if os.path.exists(INI_FILE):
        config = configparser.ConfigParser()
        config.read(INI_FILE)
        if config.has_section("default"):
            if config.has_option("default", "com-port"):
                com_port = config["default"]["com-port"]
            if config.has_option("default", "port-hint"):
                com_port_hint = config["default"]["port-hint"]

        print("Detected following parameters from config.ini file:")
        print(f"- Serial Port | com-port : {com_port}")
        print(f"- Serial Port Hint | port-hint : {com_port_hint}")
    if com_port is None or com_port == "":
        if com_port_hint is not None and com_port_hint != "":
            com_port = find_com_port_from_hint(com_port_hint)
            if com_port is None:
                print(f"No serial port found for hint {com_port_hint}")
                com_port = prompt_com_port()
        else:
            com_port = prompt_com_port()
    CFG.com_port = com_port


if __name__ == "__main__":
    main()
