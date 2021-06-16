import serial


class SerialCon:
    def __init__(self, parent):
        self.parent = parent

    def connect(self, port, baud_rate=115200):
        try:
            self._ser = serial.Serial(port, baud_rate)
            self.parent.connected = True
            print("connected to arduino on port : ", port)
            return True
        except Exception:
            print("Not able to connect on this port")
            return False

    def send_message(self, message):
        self._ser.write(message)

        for b in message:
            print(b, end=",")
        print()
