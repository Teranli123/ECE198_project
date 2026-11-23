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
                      reply JSON: {"mode": 0, "age": 45}   (the exercise every device should be on,
                      and this patient's age if the dashboard has set it)
  GET  /api/state     for the dashboard: all devices and the current exercise
  POST /api/exercise  from the dashboard, JSON: {"mode": 1}
  POST /api/age       from the dashboard, JSON: {"id": "A1B2C3", "age": 45}
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
ages = {}               # id -> patient age set on the dashboard, sent to that device


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
        age = ages.get(dev_id)
    if new:
        print(f"  + device {dev_id} connected from {request.remote_addr}")
    if age is None:
        return jsonify(mode=want)
    return jsonify(mode=want, age=age)


@app.get("/api/state")
def session_state():
    now = time.time()
    with lock:
        out = [{"id": k, "bpm": v["bpm"], "reps": v["reps"], "mode": v["mode"], "seq": v["seq"],
                "age_ms": int((now - v["last_seen"]) * 1000), "age": ages.get(k)}
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


@app.post("/api/age")
def set_age():
    data = request.get_json(silent=True) or {}
    dev_id = str(data.get("id", ""))[:32]
    try:
        age = int(data.get("age"))
    except (TypeError, ValueError):
        return jsonify(error="bad age"), 400
    if not dev_id or not 5 <= age <= 110:
        return jsonify(error="bad id or age"), 400
    with lock:
        changed = ages.get(dev_id) != age
        ages[dev_id] = age
    if changed:
        print(f"  device {dev_id}: age {age}")
    return jsonify(id=dev_id, age=age)


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
    name = f"PT Aid server {socket.gethostname()}._ptaid._tcp.local."
    current = None
    zc = None
    while True:
        ip = local_ip()
        if ip and ip != current:
            # A new address usually means a new WiFi network. Start a fresh announcer
            # bound to that network, because one created earlier keeps talking on the
            # old network only.
            try:
                if zc is not None:
                    zc.unregister_all_services()
                    zc.close()
                zc = Zeroconf(interfaces=[ip], ip_version=IPVersion.V4Only)
                info = ServiceInfo("_ptaid._tcp.local.", name, port=port,
                                   addresses=[socket.inet_aton(ip)], properties={"path": "/api/data"})
                zc.register_service(info, allow_name_change=True)
                if current:
                    print(f"  laptop address changed {current} -> {ip}; ESP32s will follow automatically")
                else:
                    print(f"  announcing on the network as {ip}:{port}")
                current = ip
            except Exception as e:   # keep the web server running even if mDNS has trouble
                print(f"  ! could not announce on the network ({ip}): {e}")
                zc = None
        time.sleep(3)


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
