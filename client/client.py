import os
import threading
import time
from typing import List

import serial
import enum

import configparser
from mod.ser import prompt_com_port, find_com_port_from_hint
from datetime import timedelta
from datetime import datetime


class Languages(enum.IntEnum):
    ENGLISH = 0
    GERMAN = 1


INI_FILE = "config.ini"


def main():
    setup_cfg_from_ini()
    print(PrintStrings.START[CFG.language])
    print(PrintStrings.CONFIG[CFG.language])
    print(f"- Serial Port | com-port : {CFG.com_port}")
    print(f"- Serial Port Hint | port-hint : {CFG.com_port_hint}")
    ser = serial.Serial(CFG.com_port, baudrate=115200)
    reply_thread = threading.Thread(target=reply_handler, args=(ser,), daemon=True)
    reply_thread.start()
    display_commands()
    while True:
        char = req_handle_cmd(ser)
        if char == "x":
            break
    print(PrintStrings.TERMINATION_PRINT[CFG.language])


def reply_handler(ser: serial.Serial):
    while True:
        reply = ser.readline()
        print()
        if reply[0] != ord("C") or reply[1] != ord("C"):
            print('Invalid reply format, must start with "CC"')
            continue
        if len(reply) == 3:
            print(f"{PrintStrings.PING_REPLY[CFG.language]}")
        elif len(reply) < 4:
            print("Invalid reply length detected")
            continue
        else:
            if reply[2] == ord(CommandChars.REQUEST):
                if reply[3] == ord(RequestChars.TIME):
                    time_str = reply[4:].rstrip("\n".encode()).decode()
                    print(f"Received current time on the ESP32: {time_str}")
            else:
                print(f"Received {reply} with no implemented reply handling")
        print(PrintStrings.REQUEST_STR[CFG.language], end="")


class Config:
    com_port = ""
    com_port_hint = ""
    language = Languages.GERMAN


CFG = Config()


CMD_PATTERN = "CC"
CMD_TERMINATION = "\n"


class CommandChars:
    MODE = "C"
    MOTOR_CTRL = "M"
    DATA = "D"
    TIME = "T"
    REQUEST = "R"


class RequestChars:
    TIME = "T"


CMD_MODE_MANUAL = "M"
CMD_MODE_NORMAL = "N"

CMD_MOTOR_PROTECTED_MODE = "P"
CMD_MOTOR_FORCE_MODE = "F"

CMD_MOTOR_CTRL_OPEN = "O"
CMD_MOTOR_CTRL_CLOSE = "C"


class PrintStrings:
    START = ["Hühnerklappen Client", "Chicken Coop Door Client"]
    CONFIG = [
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
    PING = ["Sending ping to controller", "Sende Ping an den Controller"]
    SET_TIME = [
        "Setting current time in controller",
        "Setze aktuelle Zeit im Controller",
    ]
    MOTOR_MAN_CTRL = [
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
    PING_REPLY = ["Received ping reply", "Ping Antwort bekommen"]
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


class Cmds(enum.IntEnum):
    PING_INDEX = 1
    MAN_CTRL_IDX = 2
    NORM_CTRL_IDX = 3
    SET_TIME_IDX = 4
    OPEN_PROT_IDX = 5
    CLOSE_PROT_IDX = 6
    OPEN_FORCE_IDX = 7
    CLOSE_FORCE_IDX = 8
    REQUEST_TIME = 12
    SET_MANUAL_TIME = 31
    # Set a (wrong) time at which the door should be closed. Can be used for tests
    SET_NIGHT_TIME = 32
    # Set a (wrong) time at which the door should be opened. Can be used for tests
    SET_DAY_TIME = 33


COMMANDS_STR = ["Commands:", "Kommandos:"]


class CmdStrings:
    PING = ["Send ping to ESP32-C3 controller", "Sende Ping zum ESP32-C3 Controller"]
    MAN_CTRL = [
        "Switching to manual control mode",
        "Wechsel in den manuellen Kontrollmodus",
    ]
    NORM_CTRL = [
        "Switch to Normal Control Mode",
        "Wechsel in den normalen Kontrollmodus",
    ]
    SET_TIME = [
        "Updates the time of the ESP32 controller",
        "Aktualisiert Zeit auf dem ESP32 Controller",
    ]
    PRINT_TIME_STR = [
        "Print current RTC time",
        "Zeige aktuelle RTC Zeit auf der Konsole an",
    ]
    UPDATE_TIME_MAN = [
        "Set time manually on the ESP32 controller",
        "Setze Zeit manuell auf dem ESP32 Controller",
    ]


def build_motor_ctrl_cmd_strings(close: bool, prot: bool) -> List[str]:
    strings = []
    if prot:
        prot_str = "Protected"
    else:
        prot_str = "Unprotected"
    if not close:
        dir_str = "Open"
    else:
        dir_str = "Close"
    strings.append(
        f"Motor Control {dir_str} in {prot_str} Mode. " f"Only works in Manual Mode"
    )
    if prot:
        prot_str = "geschützten"
    else:
        prot_str = "ungeschützten"
    if not close:
        dir_str = "auf"
    else:
        dir_str = "zu"
    strings.append(
        f"Klappe {dir_str} im {prot_str} Modus. "
        f"Funktioniert nur im manuellen Kontrollmodus"
    )
    return strings


CMD_INFO = {
    Cmds.PING_INDEX: [CmdStrings.PING, PrintStrings.PING],
    Cmds.MAN_CTRL_IDX: [CmdStrings.MAN_CTRL, PrintStrings.MOTOR_MAN_CTRL],
    Cmds.NORM_CTRL_IDX: [CmdStrings.NORM_CTRL, PrintStrings.MOTOR_NORMAL_STR],
    Cmds.SET_TIME_IDX: [CmdStrings.SET_TIME, PrintStrings.SET_TIME],
    Cmds.REQUEST_TIME: [CmdStrings.PRINT_TIME_STR, "HELLO"],
    Cmds.OPEN_PROT_IDX: [
        build_motor_ctrl_cmd_strings(False, True),
        PrintStrings.DOOR_OPEN_STR,
    ],
    Cmds.CLOSE_PROT_IDX: [
        build_motor_ctrl_cmd_strings(True, True),
        PrintStrings.DOOR_CLOSE_STR,
    ],
    Cmds.OPEN_FORCE_IDX: [
        build_motor_ctrl_cmd_strings(False, False),
        PrintStrings.DOOR_OPEN_STR,
    ],
    Cmds.CLOSE_FORCE_IDX: [
        build_motor_ctrl_cmd_strings(True, False),
        PrintStrings.DOOR_CLOSE_STR,
    ],
    Cmds.SET_MANUAL_TIME: [CmdStrings.SET_TIME, PrintStrings.SET_TIME],
}


def req_handle_cmd(ser: serial.Serial):
    request_cmd = input(PrintStrings.REQUEST_STR[CFG.language])
    request_cmd_l = request_cmd.lower()
    if request_cmd.lower() in ["x"]:
        return "x"
    if request_cmd.lower() in ["h"]:
        display_commands()
        return "h"
    request_cmd_d = 0
    cmd_str = ""
    if request_cmd_l.isdigit():
        request_cmd_d = int(request_cmd_l)
    if request_cmd_d in [Cmds.MAN_CTRL_IDX]:
        cmd = Cmds(request_cmd_d)
        print(f"{CMD_INFO[cmd][1][CFG.language]}")
        cmd_str = CMD_PATTERN + CommandChars.MODE + CMD_MODE_MANUAL + CMD_TERMINATION
    elif request_cmd_d in [Cmds.PING_INDEX.value]:
        cmd = Cmds(request_cmd_d)
        print(f"{CMD_INFO[cmd][1][CFG.language]}")
        cmd_str = CMD_PATTERN + CMD_TERMINATION
    elif request_cmd_d in [Cmds.REQUEST_TIME]:
        cmd_str = (
            CMD_PATTERN + CommandChars.REQUEST + RequestChars.TIME + CMD_TERMINATION
        )
    elif request_cmd_d in [Cmds.NORM_CTRL_IDX]:
        cmd = Cmds(request_cmd_d)
        print(f"{CMD_INFO[cmd][1][CFG.language]}")
        cmd_str = CMD_PATTERN + CommandChars.MODE + CMD_MODE_NORMAL + CMD_TERMINATION
    elif request_cmd_d in [Cmds.SET_TIME_IDX]:
        cmd_str = CMD_PATTERN + CommandChars.TIME
        now = datetime.now()
        # Store time without daylight saving time
        if time.localtime().tm_isdst == 1:
            now = now + timedelta(hours=-1)
        # ASCII Time Code A from CCSDS 301.0-B-4, p.19. No milliseconds accuracy
        date_time = now.strftime("%Y-%m-%dT%H:%M:%SZ")
        cmd_str += date_time + CMD_TERMINATION
        print_out = PrintStrings.SET_TIME[CFG.language] + ": " + date_time
        print(print_out)
    elif request_cmd_d in [
        Cmds.CLOSE_PROT_IDX,
        Cmds.OPEN_PROT_IDX,
        Cmds.CLOSE_FORCE_IDX,
        Cmds.OPEN_FORCE_IDX,
    ]:
        prot = False
        if request_cmd_d == Cmds.CLOSE_PROT_IDX:
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
        cmd_str = (
            CMD_PATTERN
            + CommandChars.MOTOR_CTRL
            + cmd_mode
            + dir_char
            + CMD_TERMINATION
        )
    elif request_cmd_d in [Cmds.SET_MANUAL_TIME]:
        cmd = Cmds(request_cmd_d)
        print(f"{CMD_INFO[cmd][1][CFG.language]}")
        tgt_time = prompt_time_from_user()
        # ASCII Time Code A from CCSDS 301.0-B-4, p.19. No milliseconds accuracy
        date_time = tgt_time.strftime("%Y-%m-%dT%H:%M:%SZ")
        cmd_str = CMD_PATTERN + CommandChars.TIME + date_time + CMD_TERMINATION
    else:
        print(PrintStrings.INVALID_CMD_STR[CFG.language])
    if cmd_str != "":
        ser.write(cmd_str.encode("utf-8"))


def display_commands():
    print("-" * 40)
    print(COMMANDS_STR[CFG.language])
    sorted_dict = {k: CMD_INFO[k] for k in sorted(CMD_INFO)}
    for cmd_idx, cmd in sorted_dict.items():
        print(f"{cmd_idx}: {cmd[0][CFG.language]}")


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
    d_time = datetime(
        year=year, month=month, day=day, hour=hour, minute=minute, second=second
    )
    return d_time


if __name__ == "__main__":
    main()
