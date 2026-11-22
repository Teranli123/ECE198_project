#!/usr/bin/env python3
"""PT Aid session server (Flask).

Runs on the instructor's laptop. Every ESP32 posts its heart rate and reps here,
and the dashboard (website/index.html) is served from here and shows them live.

Setup (once):  pip install flask zeroconf
Run:           python server/pt_server.py
Then open:     http://localhost:8000  on the laptop

The laptop and the ESP32s must be on the same WiFi (the phone hotspot).
The server announces itself on the WiFi with mDNS (service "_ptaid._tcp",
using the zeroconf library), so the ESP32s find the laptop on their own even
when its address changes.

API
  POST /api/data      from an ESP32, JSON: {"id": "A1B2C3", "bpm": 72.4, "reps": 5, "mode": 0}
                      reply JSON: {"mode": 0}   (the exercise every device should be on)
  GET  /api/state     for the dashboard: all devices and the current exercise
  POST /api/exercise  from the dashboard, JSON: {"mode": 1}
"""

import argparse
import logging
import os
import socket
import threading
import time

from flask import Flask, jsonify, request, send_from_directory

try:
    from zeroconf import IPVersion, ServiceInfo, Zeroconf
except ImportError:          # server still works; ESP32s then need PT_SERVER_HOST
    Zeroconf = None

HERE = os.path.dirname(os.path.abspath(__file__))
WEBSITE_DIR = os.path.normpath(os.path.join(HERE, "..", "website"))
EXERCISES = ["Squats", "Bicep curls"]

app = Flask(__name__, static_folder=WEBSITE_DIR, static_url_path="")
lock = threading.Lock()
devices = {}            # id -> {"bpm", "reps", "mode", "seq", "last_seen", "ip"}
state = {"mode": 0}     # exercise chosen on the dashboard, sent to every device


@app.get("/")
def dashboard():
    return send_from_directory(WEBSITE_DIR, "index.html")


@app.post("/api/data")
def device_data():
    data = request.get_json(silent=True) or {}
    dev_id = str(data.get("id", ""))[:32]
    if not dev_id:
        return jsonify(error="missing id"), 400
    try:
        bpm = float(data.get("bpm", 0))
        reps = int(data.get("reps", 0))
        mode = int(data.get("mode", 0))
    except (TypeError, ValueError):
        return jsonify(error="bad values"), 400
    with lock:
        new = dev_id not in devices
        seq = 0 if new else devices[dev_id]["seq"] + 1
        devices[dev_id] = {"bpm": bpm, "reps": reps, "mode": mode, "seq": seq,
                           "last_seen": time.time(), "ip": request.remote_addr}
        want = state["mode"]
    if new:
        print(f"  + device {dev_id} connected from {request.remote_addr}")
    return jsonify(mode=want)


@app.get("/api/state")
def session_state():
    now = time.time()
    with lock:
        out = [{"id": k, "bpm": v["bpm"], "reps": v["reps"], "mode": v["mode"], "seq": v["seq"],
                "age_ms": int((now - v["last_seen"]) * 1000)}
               for k, v in devices.items()]
        mode = state["mode"]
    return jsonify(mode=mode, devices=out)


@app.post("/api/exercise")
def set_exercise():
    data = request.get_json(silent=True) or {}
    try:
        mode = int(data.get("mode"))
    except (TypeError, ValueError):
        return jsonify(error="bad mode"), 400
    if not 0 <= mode < len(EXERCISES):
        return jsonify(error="unknown mode"), 400
    with lock:
        state["mode"] = mode
    print(f"  exercise for everyone -> {EXERCISES[mode]}")
    return jsonify(mode=mode)


def local_ip():
    """The laptop's address on the WiFi, for the ESP32's wifi_config.h."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.255.255.255", 1))   # no packet is sent; this just picks the outgoing interface
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return None


def advertise(port):
    """Announce this server on the WiFi so ESP32s can find it (mDNS / zeroconf).
    Re-announces with the new address whenever the laptop's address changes."""
    if Zeroconf is None:
        print("  ! zeroconf not installed (pip install zeroconf). ESP32s can't find the laptop automatically.")
        return
    zc = Zeroconf(ip_version=IPVersion.V4Only)
    name = f"PT Aid server {socket.gethostname()}._ptaid._tcp.local."
    current = None
    info = None
    while True:
        ip = local_ip()
        if ip and ip != current:
            new_info = ServiceInfo("_ptaid._tcp.local.", name, port=port,
                                   addresses=[socket.inet_aton(ip)], properties={"path": "/api/data"})
            try:
                if info is None:
                    zc.register_service(new_info, allow_name_change=True)
                else:
                    zc.update_service(new_info)
                info = new_info
                if current:
                    print(f"  laptop address changed {current} -> {ip}; ESP32s will follow automatically")
                current = ip
            except Exception as e:   # keep the web server running even if mDNS has trouble
                print(f"  ! could not announce on the network: {e}")
        time.sleep(5)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="PT Aid session server")
    ap.add_argument("--port", type=int, default=8000)
    args = ap.parse_args()

    logging.getLogger("werkzeug").setLevel(logging.WARNING)   # don't print every request
    print("PT Aid server running.")
    print(f"  Dashboard:  http://localhost:{args.port}")
    ip = local_ip()
    if ip:
        print(f"  Laptop address: {ip} (ESP32s find this automatically)")
    print("  Press Ctrl+C to stop.\n")
    threading.Thread(target=advertise, args=(args.port,), daemon=True).start()
    app.run(host="0.0.0.0", port=args.port, threaded=True)
