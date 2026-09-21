# FruitFly host controller

Runs the MaleCNS fruit-fly connectome (`flybrain`, 166,700 neurons) on the
host — laptop now, phone (Termux + Debian) later — and sends only high-level
intents to the ESP32. The firmware keeps final authority over obstacle
stops, emergency stop, motor GPIO and command timeouts.

```text
phone camera ──► Retina (vision.py) ──► LPLC2 / LC4 / LC10a
robot range  ─┘                            │
                                 FlyBrain.step() × N   (untrained connectome)
                                           │
                        DNp01 escape · DNa02 steer · DNg100 forward · MDN back
                                           │
                             decoder (fly_controller.py) ──► FORWARD / LEFT /
                                                             RIGHT / ROTATE_* / STOP
                                           │
                                  ESP32  POST /api/command
```

## Files

| File | Role |
|---|---|
| `fruitfly_bridge.py` | main loop, CLI, safety gating |
| `robot_client.py` | ESP32 HTTP client + `SimulatedRobot` for bench-free runs |
| `camera.py` | phone camera input: MJPEG URL, snapshot URL, or browser push |
| `vision.py` | retina: frame + range → population-coded drive on the fly's visual projection neurons |
| `fly_controller.py` | brain stepping, readout EMAs, decoder policy |
| `dashboard.py` + `static/` | live dashboard (`/`) and phone camera page (`/camera`) |
| `calibrate.py` | measures readout rates per stimulus; use it to tune thresholds |
| `FLYBRAIN_NOTES.md` | what the connectome model is, findings, improvement roadmap |
| `run.ps1` / `run.sh` | launchers (Windows / Linux+Termux) |

## Quick start (laptop)

```powershell
cd C:\Projects\RC\FruitFlyPrototype
.\.venv\Scripts\python.exe -m pip install -r FruitFlyHost\requirements.txt
python -m flybrain download        # once, ~260 MB into ~/fly-data

# 1. No hardware: watch the brain react to a simulated range sweep
.\FruitFlyHost\run.ps1 -Sim
# open http://localhost:8642/
```

Then with the robot powered and the laptop joined to the `RobotPrototype`
Wi-Fi AP:

```powershell
# 2. Dry run against the real robot (only STOP/heartbeat is ever sent)
python FruitFlyHost\fruitfly_bridge.py --camera push
# 3. Bench test, wheels off the floor
python FruitFlyHost\fruitfly_bridge.py --camera push --enable-motors --demo-forward --allow-turns
```

## The phone is the camera

The ESP32-CAM is disabled (`ENABLE_ESPNOW` commented out in
`RobotPrototype/config.h`). Pick one of:

**A. Browser push (no app).** Start the bridge with `--camera push`, open
`http://<host-ip>:8642/camera` on the phone, tap *Start streaming*. The page
captures the rear camera with `getUserMedia` and POSTs JPEGs to the host.
Mobile browsers only allow camera access from HTTPS or `localhost`; for a
plain-HTTP laptop host enable
`chrome://flags/#unsafely-treat-insecure-origin-as-secure` for
`http://<host-ip>:8642`. When the host runs on the phone itself this is
`http://127.0.0.1:8642/camera` and works without flags.

**B. IP Webcam app (Android).** Start the app, note its URL, then
`--camera http://<phone-ip>:8080/video` (MJPEG) or
`--camera http://<phone-ip>:8080/shot.jpg` (snapshot polling). On the phone
itself this becomes `http://127.0.0.1:8080/video`.

The phone and the host must be on the same network as the robot (join the
robot's AP; it allows several clients).

## Dashboard (`http://<host>:8642/`)

* **Anatomy** — 20k of the 166,700 neurons at their measured MaleCNS
  positions in 3-D (brain up, nerve cord down; drag to rotate). Every spike
  this tick flashes; the watched left/right populations are blue/orange.
* **Eyes** — what each half of the camera frame looks like to the retina,
  with the "object" fraction that drives the looming cells.
* **Decision** — the verb the decoder chose, why, and what was actually sent
  (dry run always sends stop).
* **Signal path** — retina → LPLC2 / LC4 / LC10a → DNp01 / DNa02, left and
  right lanes lighting with live rates. DNp01's ceiling is 0.5 spikes/step.
* **Spikes, last 20 s** — a raster of the watched populations per eye; red
  ticks on the top edge mark escape decisions.
* **Emergency stop / Clear stop** — sent straight to the ESP32.

Fonts load from Google Fonts when online and fall back to the system face
on the robot's AP; nothing else leaves the host.

If CuPy dies with `cudaErrorIllegalAddress` (seen once on this laptop), run
with `--device cpu`; the CPU path is the one the phone uses anyway.

## Safety gates

| Flag | Effect |
|---|---|
| (none) | dry-run: brain runs, dashboard live, robot only receives STOP + heartbeat |
| `--enable-motors` | decoder output is sent; still STOP unless the next two are set |
| `--demo-forward` | FORWARD allowed when nothing looms (≤ `--max-speed`, default 80/255) |
| `--allow-turns` | ROTATE away after an escape; LEFT/RIGHT from DNa02 asymmetry |

Independent of these, the ESP32 stops at ≤ 30 cm (or LD2420 presence),
latches its own e-stop, and halts if heartbeats stop for 1.5 s.

## Tuning

`python FruitFlyHost\calibrate.py` prints readout rates at rest and under
each stimulus. With the shipped brain build:

```text
condition                escape_L  escape_R  steer_L  steer_R
rest                        0.000     0.013    0.013    0.007
loom L 0.4 (LPLC2)          0.233     0.027    0.027    0.000
loom+threat L 0.8           0.900     0.027    0.000    0.020
chase L 0.6 (LC10a)         0.007     0.007    0.067    0.000
```

Escape is strongly ipsilateral and clean; steering (DNa02) is weak, hence
`--steer-threshold 0.04`. Encoder gains live in `vision.py: ENCODER`.
