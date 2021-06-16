from PyQt5.QtWidgets import QApplication, QWidget
from PyQt5.QtGui import QIcon
from PyQt5.uic import loadUi

import arduino_comm

import sys

MAX_SERVO_VALUE = 180
START_MARKER = 255
END_MARKER = 254


class ConnectDialBox(QWidget):

    def __init__(self, parent):
        super().__init__()
        self.parent = parent
        self.ui = loadUi('GUI/dial.ui', self)

        self.com_port_line_edit = self.ui.COMportlineEdit
        self.connect_button = self.ui.connect_button
        self.connect_button.clicked.connect(self.check_if_can_connect)
        self.parent.dial = True


    def check_if_can_connect(self):
        port = self.com_port_line_edit.text()
        if self.parent.ard_com.connect(port):
            self.connect()
        else:
            self.com_port_line_edit.setText("Can't connect")

    def connect(self):
        self.close()
        self.parent.set_ui()

    def closeEvent(self, event):
        self.parent.dial = False


class NerfApp(QWidget):    # main window class

    def __init__(self):
        super().__init__()

        self.ui = loadUi('GUI/nerf_turret.ui', self)
        self.show()

        self.COM_port = ""         # com port to connect to arduino
        self.dial = False          # set true when dial windows is open
        self.connected = False     # set true when connected to arduino
        self.motor_on = False      # set true to turn motor on
        self.shoot = False         # set true to shoot
        self.x = 1                 # x value 0-253 range
        self.y = 1                 # y value 0-253 range
        self.on_pad = False        # set true when cursor is oer pad
        self.ard_com = arduino_comm.SerialCon(self)  # create object handling communication with arduino

        self.pad_label = self.ui.pad_label               # pad reference created from ui file
        self.conn_button = self.ui.bluetooth_button           # bluetooth button reference created from ui file
        self.motor_on_button = self.ui.motor_on_button   # motor button reference created from ui file

        self.conn_button.clicked.connect(self.connect_dial_box)  # connect conn button to connect dial box method
        self.motor_on_button.clicked.connect(self.motor_on_off)  # connect motor button to motor_on_off method

    def connect_dial_box(self):    # create connection dialog box
        if not self.connected and not self.dial:
            dial_box = ConnectDialBox(self)
            dial_box.show()

    def set_ui(self):  # enable buttons and pad when connected
        self.motor_on_button.setEnabled(True)
        self.pad_label.setEnabled(True)
        new_button_img = QIcon('GUI/bluetooth_connect.png')
        self.conn_button.setIcon(new_button_img)

    def motor_on_off(self):    # turn motor on/off
        if self.connected:
            self.motor_on = self.motor_on_button.isChecked()
            self.set_arduino_message()

    def mouseMoveEvent(self, event):
        if self.pad_label.underMouse() and self.connected:
            x = event.x()
            y = event.y()
            if 69 < x < 551 and 69 < y < 551:   # check if cursor is over the pad
                self.x = int(self.remap(x, 70, 550, 0, MAX_SERVO_VALUE))
                self.y = int(self.remap(y, 70, 550, 0, MAX_SERVO_VALUE))
                self.on_pad = True
            else:
                self.on_pad = False
                self.shoot = False

            self.set_arduino_message()

    def mousePressEvent(self, event):
        if self.on_pad and self.motor_on:
            self.shoot = True
            self.set_arduino_message()

    def mouseReleaseEvent(self, event):
        if self.on_pad:
            self.shoot = False
            self.set_arduino_message()

    def set_arduino_message(self):
        if self.connected:
            message = bytes([START_MARKER, self.x, self.y, self.motor_on, self.shoot, END_MARKER])
            self.ard_com.send_message(message)

    @staticmethod
    def remap(x, in_min, in_max, out_min, out_max):
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min


if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = NerfApp()
    app.exec_()
