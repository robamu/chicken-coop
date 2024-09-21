#!/usr/bin/env python3
import os
import pytz
import threading
import time
from typing import List

import serial
import enum

import configparser
from mod.ser import prompt_com_port, find_com_port_from_hint
from datetime import timedelta
from datetime import datetime


INI_FILE = "config.ini"


def main():
    setup_cfg_from_ini()
    print(PrintString.START[0])
    print(PrintString.CONFIG[0])
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
    print(PrintString.TERMINATION_PRINT[0])


def reply_handler(ser: serial.Serial):
    while True:
        reply = ser.readline()
        print()
        if reply[0] != ord("C") or reply[1] != ord("C"):
            print('Invalid reply format, must start with "CC"')
            continue
        if len(reply) == 3:
            print(f"{PrintString.PING_REPLY[0]}")
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
        print(PrintString.REQUEST_STR[0], end="")


class Config:
    com_port = ""
    com_port_hint = ""


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
CMD_MOTOR_CTRL_STOP = "S"


class PrintString:
    START = ["Chicken Coop Door Client"]
    CONFIG = [
        "Detected following parameters from config.ini file:",
    ]
    REQUEST_STR = [
        "Please specify a command. 'x' to exit, 'h' to display commands: ",
    ]

    TERMINATION_PRINT = [
        "Closing Chicken Coop Door Client",
    ]
    PING = [
        "Sending ping to controller",
    ]
    SET_TIME = [
        "Setting current time in controller",
    ]
    MOTOR_MAN_CTRL = [
        "Sending command to switch to manual motor control",
    ]
    MOTOR_NORMAL_STR = [
        "Sending command to switch to normal motor control",
    ]
    DOOR_OPEN_STR_PROT = [
        "Opening door in protected mode",
    ]
    DOOR_CLOSE_STR_FORCE = [
        "Closing door in protected mode",
    ]
    DOOR_OPEN_STR_FORCE = [
        "Opening door in forced mode",
    ]
    DOOR_CLOSE_STR_FORCE = [
        "Closing door in forced mode",
    ]
    STOP_MOTOR = [
        "Stopping motor",
    ]
    INVALID_CMD_STR = ["Invalid command"]
    PING_REPLY = ["Received ping reply"]
    MANUAL_TIME_CMD_STR = ["Setting manual time"]


def get_motor_cmd_string(cmd: str, protected: bool):
    if protected:
        protect_str = "protected"
    else:
        protect_str = "unprotected"
    if cmd == CMD_MOTOR_CTRL_OPEN:
        dir_str = "Opening"
    elif cmd == CMD_MOTOR_CTRL_CLOSE:
        dir_str = "Closing"
    else:
        dir_str = "Stopping"
    return f"{dir_str} door in {protect_str} mode"


class CmdIndex(enum.IntEnum):
    INVALID = 0
    PING = 1
    MAN_CTRL = 2
    NORM_CTRL = 3
    SET_TIME = 4
    STOP_MOTOR = 5

    OPEN_PROT = 6
    CLOSE_PROT = 7

    OPEN_FORCE = 8
    CLOSE_FORCE = 9

    REQUEST_TIME = 12

    SET_MANUAL_TIME = 31
    # Set a (wrong) time at which the door should be closed. Can be used for tests
    SET_NIGHT_TIME = 32
    # Set a (wrong) time at which the door should be opened. Can be used for tests
    SET_DAY_TIME = 33


COMMANDS_STR = ["Commands:"]


class CmdString:
    PING = [
        "Send ping to ESP32-C3 controller",
    ]
    MAN_CTRL = [
        "Switching to manual control mode",
    ]
    NORM_CTRL = [
        "Switch to Normal Control Mode",
    ]
    SET_TIME = [
        "Updates the time of the ESP32 controller with the current time",
    ]
    MANUAL_TIME = [
        "Updates the time of the ESP32 controller manually",
    ]
    STOP_MOTOR = ["Stop the motor"]
    REQUEST_TIME = [
        "Print current RTC time",
    ]
    UPDATE_TIME_MAN = [
        "Set time manually on the ESP32 controller",
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
    return strings


CMD_INFO = {
    CmdIndex.PING: [CmdString.PING, PrintString.PING],
    CmdIndex.MAN_CTRL: [CmdString.MAN_CTRL, PrintString.MOTOR_MAN_CTRL],
    CmdIndex.NORM_CTRL: [CmdString.NORM_CTRL, PrintString.MOTOR_NORMAL_STR],
    CmdIndex.SET_TIME: [CmdString.SET_TIME, PrintString.SET_TIME],
    CmdIndex.REQUEST_TIME: [CmdString.REQUEST_TIME, "Requesting current time"],
    CmdIndex.OPEN_PROT: [
        build_motor_ctrl_cmd_strings(False, True),
        PrintString.DOOR_OPEN_STR_PROT,
    ],
    CmdIndex.CLOSE_PROT: [
        build_motor_ctrl_cmd_strings(True, True),
        PrintString.DOOR_CLOSE_STR_FORCE,
    ],
    CmdIndex.OPEN_FORCE: [
        build_motor_ctrl_cmd_strings(False, False),
        PrintString.DOOR_OPEN_STR_PROT,
    ],
    CmdIndex.CLOSE_FORCE: [
        build_motor_ctrl_cmd_strings(True, False),
        PrintString.DOOR_CLOSE_STR_FORCE,
    ],
    CmdIndex.STOP_MOTOR: [CmdString.STOP_MOTOR, PrintString.STOP_MOTOR],
    CmdIndex.SET_MANUAL_TIME: [
        CmdString.MANUAL_TIME,
        PrintString.MANUAL_TIME_CMD_STR,
    ],
}


def req_handle_cmd(ser: serial.Serial):
    request_cmd = input(PrintString.REQUEST_STR[0])
    request_cmd = request_cmd.lower()
    if request_cmd in ["x"]:
        return "x"
    if request_cmd in ["h"]:
        display_commands()
        return "h"
    cmd_str = ""
    request_cmd_num = 0
    if request_cmd.isdigit():
        request_cmd_num = int(request_cmd)
    if request_cmd_num in [CmdIndex.MAN_CTRL]:
        cmd = CmdIndex(request_cmd_num)
        print(f"{CMD_INFO[cmd][1]}")
        cmd_str = CMD_PATTERN + CommandChars.MODE + CMD_MODE_MANUAL + CMD_TERMINATION
    elif request_cmd_num in [CmdIndex.PING]:
        cmd = CmdIndex(request_cmd_num)
        print(f"{CMD_INFO[cmd][1]}")
        cmd_str = CMD_PATTERN + CMD_TERMINATION
    elif request_cmd_num in [CmdIndex.REQUEST_TIME]:
        cmd_str = (
            CMD_PATTERN + CommandChars.REQUEST + RequestChars.TIME + CMD_TERMINATION
        )
    elif request_cmd_num in [CmdIndex.NORM_CTRL]:
        cmd = CmdIndex(request_cmd_num)
        print(f"{CMD_INFO[cmd][1]}")
        cmd_str = CMD_PATTERN + CommandChars.MODE + CMD_MODE_NORMAL + CMD_TERMINATION
    elif request_cmd_num in [CmdIndex.SET_TIME]:
        cmd_str = CMD_PATTERN + CommandChars.TIME
        now = time_stuttgart()
        # ASCII Time Code A from CCSDS 301.0-B-4, p.19. No milliseconds accuracy
        date_time = now.strftime("%Y-%m-%dT%H:%M:%SZ")
        cmd_str += date_time + CMD_TERMINATION
        print_out = PrintString.SET_TIME[0] + ": " + date_time
        print(print_out)
    elif request_cmd_num in [
        CmdIndex.CLOSE_PROT,
        CmdIndex.OPEN_PROT,
        CmdIndex.CLOSE_FORCE,
        CmdIndex.OPEN_FORCE,
        CmdIndex.STOP_MOTOR,
    ]:
        prot = False
        if request_cmd_num in [CmdIndex.CLOSE_PROT, CmdIndex.OPEN_PROT]:
            prot = True
            cmd_mode = CMD_MOTOR_PROTECTED_MODE
        else:
            cmd_mode = CMD_MOTOR_FORCE_MODE
        if request_cmd_num in [
            CmdIndex.CLOSE_PROT,
            CmdIndex.CLOSE_FORCE,
        ]:
            dir_char = CMD_MOTOR_CTRL_CLOSE
        elif request_cmd_num in [CmdIndex.OPEN_PROT, CmdIndex.OPEN_FORCE]:
            dir_char = CMD_MOTOR_CTRL_OPEN
        else:
            dir_char = CMD_MOTOR_CTRL_STOP
        print(get_motor_cmd_string(cmd=dir_char, protected=prot))
        cmd_str = (
            CMD_PATTERN
            + CommandChars.MOTOR_CTRL
            + cmd_mode
            + dir_char
            + CMD_TERMINATION
        )
    elif request_cmd_num in [CmdIndex.SET_MANUAL_TIME]:
        cmd = CmdIndex(request_cmd_num)
        print(f"{CMD_INFO[cmd][1][0]}")
        tgt_time = prompt_time_from_user()
        # ASCII Time Code A from CCSDS 301.0-B-4, p.19. No milliseconds accuracy
        date_time = tgt_time.strftime("%Y-%m-%dT%H:%M:%SZ")
        cmd_str = CMD_PATTERN + CommandChars.TIME + date_time + CMD_TERMINATION
    else:
        print(PrintString.INVALID_CMD_STR[0])
    if cmd_str != "":
        ser.write(cmd_str.encode("utf-8"))


def time_stuttgart() -> datetime:
    stuttgart_tz = pytz.timezone("Europe/Berlin")
    now = datetime.now(stuttgart_tz)
    # Store time without daylight saving time
    if time.localtime().tm_isdst == 1:
        now = now + timedelta(hours=-1)
    return now


def display_commands():
    print("-" * 40)
    print(COMMANDS_STR[0])
    sorted_dict = {k: CMD_INFO[k] for k in sorted(CMD_INFO)}
    for cmd_idx, cmd in sorted_dict.items():
        print(f"{cmd_idx}: {cmd[0][0]}")


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
    if com_port is None or com_port == "":
        if com_port_hint is not None and com_port_hint != "":
            com_port = find_com_port_from_hint(com_port_hint)
            if com_port is None:
                print(f"No serial port found for hint {com_port_hint}")
                com_port = prompt_com_port()
        else:
            com_port = prompt_com_port()
    CFG.com_port = com_port


def prompt_time_from_user() -> datetime:
    now = time_stuttgart()
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
