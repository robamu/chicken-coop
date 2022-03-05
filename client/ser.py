
import serial
import serial.tools.list_ports


def find_com_port_from_hint(hint: str) -> (bool, str):
    """Find a COM port based on a hint string"""
    if hint == "":
        print("Invalid hint, is empty..")
        return False, ""
    ports = serial.tools.list_ports.comports()
    for port, desc, hwid in sorted(ports):
        if hint in desc:
            return True, port
    return False, ""


def prompt_com_port() -> str:
    while True:
        com_port = input(
            "Configuring serial port. Please enter COM Port"
            "(enter h to display list of COM ports): "
        )
        if com_port == "h":
            ports = serial.tools.list_ports.comports()
            for port, desc, hwid in sorted(ports):
                print("{}: {} [{}]".format(port, desc, hwid))
        else:
            if not check_port_validity(com_port):
                print(
                    "Serial port not in list of available serial ports. Try again? ([Y]/n)"
                )
                try_again = input()
                if try_again.lower() in ["y", "yes", ""]:
                    continue
                else:
                    break
            else:
                break
    return com_port


def check_port_validity(com_port_to_check: str) -> bool:
    port_list = []
    ports = serial.tools.list_ports.comports()
    for port, desc, hwid in sorted(ports):
        port_list.append(port)
    if com_port_to_check not in port_list:
        return False
    return True
