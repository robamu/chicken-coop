import os
import serial
import enum

import configparser
from mod.ser import prompt_com_port, find_com_port_from_hint
from datetime import datetime


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

CMD_MOTOR_PROTECTED_MODE = "P"
CMD_MOTOR_FORCE_MODE = "F"

CMD_MOTOR_CTRL_OPEN = "O"
CMD_MOTOR_CTRL_CLOSE = "C"


class PrintStrings:
    START_STR = ["Hühnerklappen Client", "Chicken Coop Door Client"]
    CONFIG_STR = [
        "Detected following parameters from config.ini file:",
        "Folgende Parameter wurden aus config.ini abgeleitet:",
    ]
    REQUEST_STR = [
        "Please specify a command. 'x' to exit, 'h' to display commands: ",
        "Bitte Kommando eingeben. 'x' um zu beenden, 'h' für Hilfe: ",
    ]

    TERMINATION_PRINT = [
        "Closing Chicken Coop Door Client",
        "Schließe Hühnerklappen Client",
    ]
    PING_PRINT = ["Sending ping to controller", "Sende Ping an den Controller"]
    TIME_PRINT = [
        "Setting current time in controller",
        "Setze aktuelle Zeit im Controller",
    ]
    MOTOR_MANUAL_STR = [
        "Sending command to switch to manual motor control",
        "Sende Kommando für manuelle Türkontrolle",
    ]
    MOTOR_NORMAL_STR = [
        "Sending command to switch to normal motor control",
        "Sende Kommando für normale Türkontrolle",
    ]
    DOOR_OPEN_STR = [
        "Opening door in protected mode",
        "Öffne Klappe im geschützten Modus",
    ]
    DOOR_CLOSE_STR = [
        "Closing door in protected mode",
    ]
    INVALID_CMD_STR = ["Invalid command", "Ungültiges Kommando"]
    MANUAL_TIME_CMD_STR = [
        "Setting manual time",
        "Setze manuelle Uhrzeit"
    ]


def get_door_open_close_str(close: bool, protected: bool):
    if CFG.language == Languages.GERMAN:
        if protected:
            protect_str = "geschützten"
        else:
            protect_str = "ungeschützten"
        if close:
            dir_str = "Schließe"
        else:
            dir_str = "Öffne"
        return f"{dir_str} Klappe im {protect_str} Modus"
    elif CFG.language == Languages.ENGLISH:
        if protected:
            protect_str = "protected"
        else:
            protect_str = "unprotected"
        if close:
            dir_str = "Closing"
        else:
            dir_str = "Opening"
        return f"{dir_str} door in {protect_str} mode"
    return ""


class Cmds:
    PING_INDEX = 1
    MAN_CTRL_IDX = 2
    NORM_CTRL_IDX = 3
    SET_TIME_IDX = 4
    OPEN_IDX = 5
    CLOSE_IDX = 6
    SET_MANUAL_TIME = 31
    # Set a (wrong) time at which the door should be closed. Can be used for tests
    SET_NIGHT_TIME = 32
    # Set a (wrong) time at which the door should be opened. Can be used for tests
    SET_DAY_TIME = 33


def main():
    setup_cfg_from_ini()
    print(PrintStrings.START_STR[CFG.language])
    print(PrintStrings.CONFIG_STR[CFG.language])
    print(f"- Serial Port | com-port : {CFG.com_port}")
    print(f"- Serial Port Hint | port-hint : {CFG.com_port_hint}")
    ser = serial.Serial(CFG.com_port, baudrate=115200)
    display_commands(CFG.language)
    while True:
        request_cmd = input(PrintStrings.REQUEST_STR[CFG.language])
        if request_cmd.lower() in ["x"]:
            break
        if request_cmd.lower() in ["h"]:
            display_commands(CFG.language)
            continue
        if request_cmd.lower() in [str(Cmds.MAN_CTRL_IDX)]:
            print(PrintStrings.MOTOR_MANUAL_STR[CFG.language])
            cmd = CMD_PATTERN + CMD_MODE + CMD_MODE_MANUAL + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(Cmds.PING_INDEX)]:
            print(PrintStrings.PING_PRINT[CFG.language])
            cmd = CMD_PATTERN + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(Cmds.NORM_CTRL_IDX)]:
            print(PrintStrings.MOTOR_NORMAL_STR[CFG.language])
            cmd = CMD_PATTERN + CMD_MODE + CMD_MODE_NORMAL + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(Cmds.SET_TIME_IDX)]:
            cmd = CMD_PATTERN + CMD_TIME
            now = datetime.now()
            # ASCII Time Code A from CCSDS 301.0-B-4, p.19. No milliseconds accuracy
            date_time = now.strftime("%Y-%m-%dT%H:%M:%SZ")
            cmd += date_time + CMD_TERMINATION
            print_out = PrintStrings.TIME_PRINT[CFG.language] + ": " + date_time
            print(print_out)
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(Cmds.OPEN_IDX)]:
            print(get_door_open_close_str(close=False, protected=True))
            cmd = (
                CMD_PATTERN
                + CMD_MOTOR_CTRL
                + CMD_MOTOR_PROTECTED_MODE
                + CMD_MOTOR_CTRL_OPEN
                + CMD_TERMINATION
            )
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(Cmds.CLOSE_IDX)]:
            print(get_door_open_close_str(close=True, protected=True))
            cmd = (
                CMD_PATTERN
                + CMD_MOTOR_CTRL
                + CMD_MOTOR_PROTECTED_MODE
                + CMD_MOTOR_CTRL_OPEN
                + CMD_TERMINATION
            )
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(Cmds.SET_MANUAL_TIME)]:
            print(PrintStrings.MANUAL_TIME_CMD_STR[CFG.language])
            now = datetime.now()
            year = input("Enter Year [nothing for current year]: ")
            if year == "":
                year = now.year
            month = input("Enter Month [1-12 or nothing for current month]:")
            if month == "":
                month = now.month
            day = input("Enter month day [0-31 or nothing for current day]: ")
            if day == "":
                day = now.day
            hour = input("Enter hour of day [0-24 or nothing for current hour]: ")
            if hour == "":
                hour = now.hour
            minute = input("Enter minute of the hour [0-60 or nothing for current minute]: ")
            if minute == "":
                minute = now.minute
            second = input("Enter second of the minute [0-60 or nothing for current minute: ")
            if second == "":
                second = now.second
            time = datetime(
                year=year,
                month=month,
                day=day,
                hour=hour,
                minute=minute,
                second=second
            )
            # ASCII Time Code A from CCSDS 301.0-B-4, p.19. No milliseconds accuracy
            date_time = time.strftime("%Y-%m-%dT%H:%M:%SZ")
            cmd = CMD_PATTERN + CMD_TIME + date_time + CMD_TERMINATION
            ser.write(cmd.encode('utf-8'))
        else:
            print(PrintStrings.INVALID_CMD_STR[CFG.language])

    print(PrintStrings.TERMINATION_PRINT[CFG.language])


def display_commands(language: Languages):
    print("-" * 40)
    if language == Languages.ENGLISH:
        print("Commands:")
        print(f"{Cmds.PING_INDEX}: Send ping to ESP32 controller")
        print(f"{Cmds.MAN_CTRL_IDX}: Switch to Manual Control Mode")
        print(f"{Cmds.NORM_CTRL_IDX}: Switch to Normal Control Mode")
        print(f"{Cmds.SET_TIME_IDX}: Updates the Time of the ESP32 controller")
        print(f"{Cmds.OPEN_IDX}: Motor Control Open. Only works in manual mode")
        print(f"{Cmds.CLOSE_IDX}: Motor Control Close. Only works in normal mode")
    elif language == Languages.GERMAN:
        print("Kommandos:")
        print(f"{Cmds.PING_INDEX}: Ping zum ESP32 Controller senden")
        print(f"{Cmds.MAN_CTRL_IDX}: Wechsel in den manuellen Kontrollmodus")
        print(f"{Cmds.NORM_CTRL_IDX}: Wechsel in den normalen Kontrollmodus")
        print(f"{Cmds.SET_TIME_IDX}: Aktualisiert die Zeit des ESP32 Controller")
        print(
            f"{Cmds.OPEN_IDX}: Klappe auf. Funktioniert nur im manuellen Kontrollmodus"
        )
        print(
            f"{Cmds.CLOSE_IDX}: Klappe zu. Funktioniert nur im manuellen Kontrollmodus"
        )


def setup_cfg_from_ini():
    com_port = None
    com_port_hint = None
    language_shortcode = None
    if os.path.exists(INI_FILE):
        config = configparser.ConfigParser()
        config.read(INI_FILE)
        if config.has_section("default"):
            if config.has_option("default", "com-port"):
                com_port = config["default"]["com-port"]
            if config.has_option("default", "port-hint"):
                com_port_hint = config["default"]["port-hint"]
            if config.has_option("default", "language"):
                language_shortcode = config["default"]["language"]
    if com_port is None or com_port == "":
        if com_port_hint is not None and com_port_hint != "":
            com_port = find_com_port_from_hint(com_port_hint)
            if com_port is None:
                print(f"No serial port found for hint {com_port_hint}")
                com_port = prompt_com_port()
        else:
            com_port = prompt_com_port()
    CFG.language = Languages.ENGLISH
    if language_shortcode is not None:
        if language_shortcode == "de":
            CFG.language = Languages.GERMAN
    CFG.com_port = com_port


if __name__ == "__main__":
    main()
