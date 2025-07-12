import threading
import socket
import ssl
import tkinter as tk
from tkinter.scrolledtext import ScrolledText

import paho.mqtt.client as mqtt

# MQTT configuration - mirrors telemetry_config.h
MQTT_HOST = "5aeaff002e7c423299c2d92361292d54.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "yousef"
MQTT_PASS = "Yousef123"
MQTT_TOPIC = "com/yousef/esp32/data"

# UDP configuration - listen on all interfaces
UDP_PORT = 19132


def format_data(data: bytes) -> str:
    """Return a readable representation of received data."""
    try:
        text = data.decode().strip()
        if text:
            return text
    except UnicodeDecodeError:
        pass
    return data.hex()


class ReceiverGUI:
    def __init__(self, master: tk.Tk):
        self.master = master
        master.title("Telemetry Receiver")
        self.text = ScrolledText(master, width=80, height=20)
        self.text.pack(fill=tk.BOTH, expand=True)

    def display(self, source: str, message: str) -> None:
        self.text.insert(tk.END, f"[{source}] {message}\n")
        self.text.see(tk.END)


class MqttListener:
    def __init__(self, gui: ReceiverGUI):
        self.gui = gui
        self.client = mqtt.Client()
        self.client.username_pw_set(MQTT_USER, MQTT_PASS)
        self.client.tls_set_context(ssl.create_default_context())
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.connect(MQTT_HOST, MQTT_PORT)

    def on_connect(self, client, userdata, flags, rc, properties=None):
        client.subscribe(MQTT_TOPIC)

    def on_message(self, client, userdata, msg):
        text = format_data(msg.payload)
        self.gui.display("MQTT", text)

    def start(self):
        self.client.loop_start()


class UdpListener(threading.Thread):
    def __init__(self, gui: ReceiverGUI):
        super().__init__(daemon=True)
        self.gui = gui
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("", UDP_PORT))

    def run(self):
        while True:
            data, addr = self.sock.recvfrom(4096)
            text = format_data(data)
            self.gui.display(f"UDP {addr[0]}:{addr[1]}", text)


def main():
    root = tk.Tk()
    gui = ReceiverGUI(root)
    mqtt_listener = MqttListener(gui)
    mqtt_listener.start()
    udp_listener = UdpListener(gui)
    udp_listener.start()
    root.mainloop()


if __name__ == "__main__":
    main()
