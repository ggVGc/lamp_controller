import serial
import time
import struct


def wait():
    time.sleep(2)


with serial.Serial("/dev/ttyACM1", 115200, exclusive=False) as port:
    def set_color(r, g, b):
        print(f"Setting color to {r}, {g}, {b}")
        port.write(struct.pack("<BBBB", 0, r, g, b))

    def set_level(level):
        print(f"Setting level: {level}")
        port.write(struct.pack("<BB", 1, level))

    # # port.write(b'red')
    # # wait()
    # port.write(b"color")
    # time.sleep(1)

    # Day?
    # set_color(255, 80, 20)

    # Wramer day
    # set_color(255, 70, 20)

    # Afternoon
    # set_color(255, 50, 20)

    # Night?
    # set_color(255, 0, 0)
    set_level(220)


    print("reading responses")
    while True:
        res = port.readline()
        print(res)
