"""
GUI dashboard for MQTT telemetry.

This script subscribes to the MQTT topic used by the ESP32 and decodes
binary CAN frames published by the firmware. The field layout mirrors
the decoding done in main.c when logging CAN data to the SD card.
"""

import ssl
import struct
import threading
import time
import tkinter as tk
from tkinter import ttk, messagebox

import paho.mqtt.client as mqtt


# Default connection parameters (match telemetry_config.h)
DEFAULTS = {
    "broker": "5aeaff002e7c423299c2d92361292d54.s1.eu.hivemq.cloud",
    "port": 8883,
    "username": "yousef",
    "password": "Yousef123",
    "topic": "com/yousef/esp32/data",
}

# CAN message identifiers (see logging.h)
COMM_CAN_ID_IMU_ANGLE = 0x004
COMM_CAN_ID_IMU_ACCEL = 0x005
COMM_CAN_ID_ADC = 0x006
COMM_CAN_ID_PROX_ENCODER = 0x007
COMM_CAN_ID_GPS_LATLONG = 0x008
COMM_CAN_ID_TEMP = 0x009

# Fields logged by the ESP32 firmware (see logging.c CSV header)
FIELDS = [
    "SUS_1",
    "SUS_2",
    "SUS_3",
    "SUS_4",
    "PRESSURE_1",
    "PRESSURE_2",
    "RPM_FL",
    "RPM_FR",
    "RPM_RL",
    "RPM_RR",
    "ENC_ANGLE",
    "IMU_Ang_X",
    "IMU_Ang_Y",
    "IMU_Ang_Z",
    "IMU_Accel_X",
    "IMU_Accel_Y",
    "IMU_Accel_Z",
    "Temp_FL",
    "Temp_FR",
    "Temp_RL",
    "Temp_RR",
    "GPS_Long",
    "GPS_Lat",
]

# twai_message_t frame layouts. Different ESP-IDF versions pack the
# structure slightly differently, so support both 24-byte (with padding)
# and 21-byte variants.
TWAI_STRUCT_PAD = struct.Struct("<IIB3x8sI")  # 24 bytes
TWAI_STRUCT_NOPAD = struct.Struct("<IIB8sI")   # 21 bytes


class Dashboard(ttk.Frame):
    def __init__(self, master: tk.Tk):
        super().__init__(master)
        self.vars = {}
        for row, label in enumerate(FIELDS):
            ttk.Label(self, text=label + ":", width=20, anchor="w").grid(row=row, column=0, sticky="w", padx=5, pady=2)
            var = tk.StringVar(value="--")
            ttk.Label(self, textvariable=var, width=15, anchor="w", relief="sunken", background="white").grid(row=row, column=1, sticky="w", padx=5, pady=2)
            self.vars[label] = var
        for i in range(len(FIELDS)):
            self.rowconfigure(i, weight=0)
        self.columnconfigure(1, weight=1)

    def update_field(self, field: str, value) -> None:
        if field in self.vars:
            self.vars[field].set(str(value))


class TelemetryApp:
    def __init__(self, master: tk.Tk):
        self.master = master
        master.title("MQTT Telemetry Dashboard")
        master.protocol("WM_DELETE_WINDOW", self.close)

        self.settings_frame = ttk.Frame(master)
        self.settings_frame.pack(fill="both", expand=True, padx=10, pady=10)

        self.broker_var = tk.StringVar(value=DEFAULTS["broker"])
        self.port_var = tk.IntVar(value=DEFAULTS["port"])
        self.user_var = tk.StringVar(value=DEFAULTS["username"])
        self.pass_var = tk.StringVar(value=DEFAULTS["password"])
        self.topic_var = tk.StringVar(value=DEFAULTS["topic"])

        ttk.Label(self.settings_frame, text="Broker").grid(row=0, column=0, sticky="w")
        ttk.Entry(self.settings_frame, textvariable=self.broker_var, width=40).grid(row=0, column=1, sticky="ew")
        ttk.Label(self.settings_frame, text="Port").grid(row=1, column=0, sticky="w")
        ttk.Entry(self.settings_frame, textvariable=self.port_var).grid(row=1, column=1, sticky="ew")
        ttk.Label(self.settings_frame, text="Username").grid(row=2, column=0, sticky="w")
        ttk.Entry(self.settings_frame, textvariable=self.user_var).grid(row=2, column=1, sticky="ew")
        ttk.Label(self.settings_frame, text="Password").grid(row=3, column=0, sticky="w")
        ttk.Entry(self.settings_frame, textvariable=self.pass_var, show="*").grid(row=3, column=1, sticky="ew")
        ttk.Label(self.settings_frame, text="Topic").grid(row=4, column=0, sticky="w")
        ttk.Entry(self.settings_frame, textvariable=self.topic_var, width=40).grid(row=4, column=1, sticky="ew")

        self.connect_btn = ttk.Button(self.settings_frame, text="Connect", command=self.connect)
        self.connect_btn.grid(row=5, column=1, sticky="e", pady=(5, 0))

        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(self.settings_frame, textvariable=self.status_var, foreground="blue").grid(row=6, column=0, columnspan=2, sticky="w", pady=(5, 0))

        for i in range(2):
            self.settings_frame.columnconfigure(i, weight=1)

        self.client = None
        self.dashboard = None
        self.decoder_lock = threading.Lock()
        self.latest = {field: "--" for field in FIELDS}

    # MQTT callbacks -------------------------------------------------------
    def on_connect(self, client, userdata, flags, rc, properties=None):
        if rc == 0:
            client.subscribe(self.topic_var.get())
            self.status_var.set("Connected")
            self.master.after(0, self.show_dashboard)
        else:
            self.status_var.set(f"Connect failed ({rc})")
            self.connect_btn.config(state="normal")

    def on_disconnect(self, client, userdata, rc, properties=None):
        self.status_var.set("Disconnected")
        self.connect_btn.config(state="normal")

    def on_message(self, client, userdata, msg):
        payload = msg.payload
        offset = 0
        while offset < len(payload):
            try:
                identifier, data, used = self.decode_twai_message(payload[offset:])
            except Exception as exc:
                print(f"Failed to decode frame: {exc}")
                break
            self.process_can_frame(identifier, data)
            offset += used

    # MQTT management ------------------------------------------------------
    def connect(self) -> None:
        broker = self.broker_var.get().strip()
        port = int(self.port_var.get())
        if not broker or port <= 0:
            messagebox.showerror("Error", "Invalid broker/port")
            return

        self.status_var.set("Connecting...")
        self.connect_btn.config(state="disabled")
        self.master.update()

        self.client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
        if self.user_var.get():
            self.client.username_pw_set(self.user_var.get(), self.pass_var.get())
        ctx = ssl.create_default_context()
        self.client.tls_set_context(ctx)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        self.client.connect(broker, port, keepalive=60)
        self.client.loop_start()

        # wait briefly for connection
        start = time.time()
        while self.client.is_connected() is False and (time.time() - start) < 5:
            time.sleep(0.1)
            self.master.update()
        if not self.client.is_connected():
            self.status_var.set("Connection timeout")
            self.connect_btn.config(state="normal")

    def disconnect(self) -> None:
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()
            self.client = None

    def close(self) -> None:
        self.disconnect()
        self.master.destroy()

    # Dashboard ------------------------------------------------------------
    def show_dashboard(self) -> None:
        if self.dashboard:
            self.dashboard.destroy()
        self.settings_frame.pack_forget()
        self.dashboard = Dashboard(self.master)
        self.dashboard.pack(fill="both", expand=True, padx=10, pady=10)
        ttk.Button(self.dashboard, text="Back", command=self.back).grid(row=0, column=2, sticky="e", padx=5, pady=2)

    def back(self) -> None:
        self.disconnect()
        if self.dashboard:
            self.dashboard.destroy()
            self.dashboard = None
        self.settings_frame.pack(fill="both", expand=True)

    # CAN frame handling ---------------------------------------------------
    def decode_twai_message(self, payload: bytes):
        """Decode one twai_message_t frame from payload.

        Returns a tuple of (identifier, data bytes, used_length).
        The function supports both 24 byte (padded) and 21 byte frame
        layouts as well as a minimal variant without timestamp.
        """
        if len(payload) >= TWAI_STRUCT_PAD.size:
            flags, identifier, dlc, data, _ts = TWAI_STRUCT_PAD.unpack_from(payload)
            used = TWAI_STRUCT_PAD.size
        elif len(payload) >= TWAI_STRUCT_NOPAD.size:
            flags, identifier, dlc, data, _ts = TWAI_STRUCT_NOPAD.unpack_from(payload)
            used = TWAI_STRUCT_NOPAD.size
        elif len(payload) >= 9:  # Flags(4) + ID(4) + DLC(1)
            flags, identifier, dlc = struct.unpack_from("<IIB", payload)
            if len(payload) < 9 + dlc:
                raise ValueError("payload too short for data")
            data = payload[9:9 + dlc].ljust(8, b"\x00")
            used = 9 + dlc
        else:
            raise ValueError("payload too short")
        return identifier, data[:dlc], used

    def process_can_frame(self, identifier: int, data: bytes) -> None:
        with self.decoder_lock:
            if identifier == COMM_CAN_ID_PROX_ENCODER:
                val = int.from_bytes(data.ljust(8, b"\x00"), "little")
                rpm_fl = val & 0x7FF
                rpm_fr = (val >> 11) & 0x7FF
                rpm_rl = (val >> 22) & 0x7FF
                rpm_rr = (val >> 33) & 0x7FF
                enc = (val >> 44) & 0x3FF
                # Speed field exists but is not logged in CSV
                self.latest["RPM_FL"] = rpm_fl
                self.latest["RPM_FR"] = rpm_fr
                self.latest["RPM_RL"] = rpm_rl
                self.latest["RPM_RR"] = rpm_rr
                self.latest["ENC_ANGLE"] = enc

            elif identifier == COMM_CAN_ID_ADC:
                val = int.from_bytes(data.ljust(8, b"\x00"), "little")
                sus1 = val & 0x3FF
                sus2 = (val >> 10) & 0x3FF
                sus3 = (val >> 20) & 0x3FF
                sus4 = (val >> 30) & 0x3FF
                pressure1 = (val >> 40) & 0x3FF
                pressure2 = (val >> 50) & 0x3FF
                self.latest["SUS_1"] = sus1
                self.latest["SUS_2"] = sus2
                self.latest["SUS_3"] = sus3
                self.latest["SUS_4"] = sus4
                self.latest["PRESSURE_1"] = pressure1
                self.latest["PRESSURE_2"] = pressure2

            elif identifier == COMM_CAN_ID_IMU_ANGLE:
                if len(data) >= 6:
                    x, y, z = struct.unpack_from("<hhh", data)
                    self.latest["IMU_Ang_X"] = x
                    self.latest["IMU_Ang_Y"] = y
                    self.latest["IMU_Ang_Z"] = z

            elif identifier == COMM_CAN_ID_GPS_LATLONG:
                if len(data) >= 8:
                    lon, lat = struct.unpack_from("<ff", data)
                    self.latest["GPS_Long"] = f"{lon:.6f}"
                    self.latest["GPS_Lat"] = f"{lat:.6f}"

            elif identifier == COMM_CAN_ID_IMU_ACCEL:
                if len(data) >= 6:
                    x, y, z = struct.unpack_from("<hhh", data)
                    self.latest["IMU_Accel_X"] = x
                    self.latest["IMU_Accel_Y"] = y
                    self.latest["IMU_Accel_Z"] = z

            elif identifier == COMM_CAN_ID_TEMP:
                if len(data) >= 8:
                    tfl, tfr, trl, trr = struct.unpack_from("<hhhh", data)
                    self.latest["Temp_FL"] = tfl
                    self.latest["Temp_FR"] = tfr
                    self.latest["Temp_RL"] = trl
                    self.latest["Temp_RR"] = trr

            # Update dashboard
            if self.dashboard:
                for field, value in self.latest.items():
                    self.dashboard.update_field(field, value)


# ---------------------------------------------------------------------------
if __name__ == "__main__":
    root = tk.Tk()
    app = TelemetryApp(root)
    root.mainloop()
