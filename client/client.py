import serial
from ser import prompt_com_port


def main():
    com_port = prompt_com_port()
    ser = serial.Serial(com_port, baudrate=115200)
    ser.write(b'CC')
    pass

   
if __name__ == "__main__":
    main()
