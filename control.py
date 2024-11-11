import serial
import time
import struct

def wait():
    time.sleep(2)

with serial.Serial('/dev/ttyACM0', 115200) as port:
    # port.write(b'red')
    # wait()
    port.write(b'color')
    time.sleep(1)
    port.write(struct.pack("<BBB", 255, 100, 0))

    print("reading responses")
    while True:
        res = port.readline()
        print(res)


