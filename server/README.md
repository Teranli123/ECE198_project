# Running a live rehab session

The ESP32s send heart rate and reps over WiFi to a small server on the
instructor's laptop. The laptop serves the dashboard, which shows every patient
live.

```
ESP32 (each patient) ──WiFi──▶  laptop: server/pt_server.py  ──▶  dashboard in the browser
         ▲                                  │
         └──── "exercise for everyone" ◀────┘   (sent back in each reply)
```

Everything happens on the phone hotspot, so no internet is needed and patient
data stays on the laptop.

## 1. Phone hotspot

- Turn on the hotspot. Note its name and password.
- **iPhone:** turn on *Maximize Compatibility* (the ESP32 only does 2.4 GHz WiFi).
- **Android:** set the band to 2.4 GHz if there is a choice.
- Connect the laptop to the hotspot. Eduroam won't work for this.

## 2. Start the server on the laptop

Needs Python 3.8 or newer and Flask (install it once):

```
pip install flask
cd ECE198_project
python server/pt_server.py
```

It prints something like:

```
PT Aid server running.
  Dashboard:  http://localhost:8000
  Put this in the ESP32's wifi_config.h as PT_SERVER_HOST:  172.20.10.2
```

Open **http://localhost:8000** in a browser on the laptop. It says "Waiting for
devices" until an ESP32 connects.

The first time, Windows may ask whether Python can use the network. Allow it on
**private** networks, or the ESP32s can't reach the laptop. If you clicked
Cancel, use *Windows Security → Firewall → Allow an app through firewall* and
tick Python for Private.

## 3. Point the ESP32 at the laptop

1. Copy `esp32_pt_aid/src/wifi_config.example.h` to `esp32_pt_aid/src/wifi_config.h`.
2. Fill in the hotspot name, password, and the address the server printed.
   (`wifi_config.h` is ignored by git, so the password doesn't end up on GitHub.)
3. Upload with PlatformIO (`pio run -t upload`).
4. In the serial monitor you should see
   `WiFi connected as device A1B2C3, IP ..., sending to http://172.20.10.2:8000/api/data`.
   The server prints `+ device A1B2C3 connected` and a tile appears on the dashboard.

The 6-character device ID comes from the board's chip, so it stays the same
every time. Click the pencil on the tile to give it the patient's name, age and
weights. These are saved in that browser and keyed to the device ID.

## Troubleshooting

| What you see | Likely cause |
|---|---|
| ESP32 never prints "WiFi connected" | Wrong hotspot name or password, or the hotspot is 5 GHz only |
| ESP32 connects but no tile appears | Wrong `PT_SERVER_HOST`, or Windows Firewall is blocking Python |
| The laptop's address changed | Hotspots can hand out a new address after reconnecting. Re-check what the server prints and update `wifi_config.h` |
| Tile is greyed out with "No signal" | That ESP32 hasn't sent anything for 3 seconds (out of range, unplugged, or rebooting) |

## Excel export

"Export to Excel" on the dashboard uses the ExcelJS library, which is saved in
`website/vendor/exceljs.min.js`, so it works without internet.

## Trying it without devices

- Opening `website/index.html` directly (double-click) shows demo data.
- `http://localhost:8000/?demo` shows demo data even with the server running.
