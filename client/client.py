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
    MANUAL_TIME_CMD_STR = ["Setting manual time", "Setze manuelle Uhrzeit"]


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
    OPEN_PROT_IDX = 5
    CLOSE_PROT_IDX = 6
    OPEN_FORCE_IDX = 7
    CLOSE_FORCE_IDX = 8
    SET_MANUAL_TIME = 31
    # Set a (wrong) time at which the door should be closed. Can be used for tests
    SET_NIGHT_TIME = 32
    # Set a (wrong) time at which the door should be opened. Can be used for tests
    SET_DAY_TIME = 33


class CmdStrings:
    COMMANDS_STR = [
        "Commands:",
        "Kommandos:"
    ]
    PING_STRING = [
        "Send ping to ESP32-C3 controller",
        "Sende Ping zum ESP32-C3 Controller"
    ]
    MAN_CTRL_STRING = [
        "Switching to manual control mode",
        "Wechsel in den manuellen Kontrollmodus"
    ]
    NORM_CTRL_STRING = [
        "Switch to Normal Control Mode",
        "Wechsel in den normalen Kontrollmodus"
    ]
    UPDATE_TIME_STRING = [
        "Updates the time of the ESP32 controller",
        "Aktualisiert Zeit aug dem ESP32 Controller"
    ]
    UPDATE_TIME_MAN_STRING = [
        "Set time mnaually on the ESP32 controller",
        "Setze Zeit manuell auf dem ESP32 Controller"
    ]


def print_motor_ctrl_cmd_string(cmd_idx: int, lang: Languages, close: bool, prot: bool):
    if lang == Languages.ENGLISH:
        if prot:
            prot_str = "Protected"
        else:
            prot_str = "Unprotected"
        if not close:
            dir_str = "Open"
        else:
            dir_str = "Close"
        print(f"{cmd_idx}: Motor Control {dir_str} in {prot_str} Mode. "
              f"Only works in Manual Mode")
    elif lang == Languages.GERMAN:
        if prot:
            prot_str = "geschützten"
        else:
            prot_str = "ungeschützten"
        if not close:
            dir_str = "auf"
        else:
            dir_str = "zu"
        print(
            f"{cmd_idx}: Klappe {dir_str} im {prot_str} Modus. "
            f"Funktioniert nur im manuellen Kontrollmodus"
        )


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
        elif request_cmd.lower() in [
            str(Cmds.CLOSE_PROT_IDX),
            str(Cmds.OPEN_PROT_IDX),
            str(Cmds.CLOSE_FORCE_IDX),
            str(Cmds.OPEN_FORCE_IDX)
        ]:
            prot = False
            if request_cmd.lower() == Cmds.CLOSE_PROT_IDX:
                prot = True
                cmd_mode = CMD_MOTOR_PROTECTED_MODE
            else:
                cmd_mode = CMD_MOTOR_FORCE_MODE
            if request_cmd.lower() in [
                str(Cmds.CLOSE_PROT_IDX),
                str(Cmds.CLOSE_FORCE_IDX),
            ]:
                dir_char = CMD_MOTOR_CTRL_CLOSE
                close = True
            else:
                dir_char = CMD_MOTOR_CTRL_OPEN
                close = False
            print(get_door_open_close_str(close=close, protected=prot))
            cmd = CMD_PATTERN + CMD_MOTOR_CTRL + cmd_mode + dir_char + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
        elif request_cmd.lower() in [str(Cmds.SET_MANUAL_TIME)]:
            print(PrintStrings.MANUAL_TIME_CMD_STR[CFG.language])
            time = prompt_time_from_user()
            # ASCII Time Code A from CCSDS 301.0-B-4, p.19. No milliseconds accuracy
            date_time = time.strftime("%Y-%m-%dT%H:%M:%SZ")
            cmd = CMD_PATTERN + CMD_TIME + date_time + CMD_TERMINATION
            ser.write(cmd.encode("utf-8"))
        else:
            print(PrintStrings.INVALID_CMD_STR[CFG.language])

    print(PrintStrings.TERMINATION_PRINT[CFG.language])


def display_commands(language: Languages):
    print("-" * 40)
    print(CmdStrings.COMMANDS_STR[CFG.language])
    print(f"{Cmds.PING_INDEX}: {CmdStrings.PING_STRING[CFG.language]}")
    print(f"{Cmds.MAN_CTRL_IDX}: {CmdStrings.MAN_CTRL_STRING[CFG.language]}")
    print(f"{Cmds.NORM_CTRL_IDX}: {CmdStrings.NORM_CTRL_STRING[CFG.language]}")
    print(f"{Cmds.SET_TIME_IDX}: {CmdStrings.UPDATE_TIME_STRING[CFG.language]}")
    print_motor_ctrl_cmd_string(
        cmd_idx=Cmds.OPEN_PROT_IDX,
        close=False,
        lang=CFG.language,
        prot=True
    )
    print_motor_ctrl_cmd_string(
        cmd_idx=Cmds.CLOSE_PROT_IDX,
        close=True,
        lang=CFG.language,
        prot=True
    )
    print_motor_ctrl_cmd_string(
        cmd_idx=Cmds.OPEN_FORCE_IDX,
        close=False,
        lang=CFG.language,
        prot=False
    )
    print_motor_ctrl_cmd_string(
        cmd_idx=Cmds.CLOSE_FORCE_IDX,
        close=True,
        lang=CFG.language,
        prot=False
    )
    print(f"{Cmds.SET_MANUAL_TIME}: {CmdStrings.UPDATE_TIME_MAN_STRING[CFG.language]}")


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


def prompt_time_from_user() -> datetime:
    now = datetime.now()
    while True:
        year = input("Enter Year [nothing for current year]: ")
        if year == "":
            year = now.year
            break
        if not year.isdigit():
            print("Invalid year")
            continue
        year = int(year)
        break
    while True:
        month = input("Enter Month [1-12 or nothing for current month]:")
        if month == "":
            month = now.month
            break
        if not month.isdigit():
            print("Invalid month")
            continue
        month = int(month)
        if month < 1 or month > 12:
            print("Invalid month")
            continue
        break
    while True:
        day = input("Enter month day [1-31 or nothing for current day]: ")
        if day == "":
            day = now.day
            break
        if not day.isdigit():
            print("Invalid day")
            continue
        day = int(day)
        if day < 1 or day > 31:
            print("Invalid day")
            continue
        break
    while True:
        hour = input("Enter hour of day [0-24 or nothing for current hour]: ")
        if hour == "":
            hour = now.hour
            break
        if not hour.isdigit():
            print("Invalid hour")
            continue
        hour = int(hour)
        if hour > 24 or hour < 0:
            print("Invalid hour")
            continue
        break
    while True:
        minute = input(
            "Enter minute of the hour [0-60 or nothing for current minute]: "
        )
        if minute == "":
            minute = now.minute
            break
        if not minute.isdigit():
            print("Invalid minute")
            continue
        minute = int(minute)
        if minute > 60 or minute < 0:
            print("Invalid minute")
            continue
        break
    while True:
        second = input(
            "Enter second of the minute [0-60 or nothing for current minute]: "
        )
        if second == "":
            second = now.second
            break
        if not second.isdigit():
            print("Invalid seconds")
            continue
        second = int(second)
        if second < 0 or second > 60:
            print("Invalid seconds")
            continue
        break
    time = datetime(
        year=year, month=month, day=day, hour=hour, minute=minute, second=second
    )
    return time


if __name__ == "__main__":
    main()
