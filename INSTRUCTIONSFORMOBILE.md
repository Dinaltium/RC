# Instructions for Claude running inside Termux → Debian (proot) on the phone

You are setting up this repo so the **phone** runs the fruit-fly brain and
drives the ESP32 robot, replacing the laptop. Work through the steps in
order, verify each one, and report what you did. Ask the user only for
things you cannot check yourself (which Wi-Fi the phone is on, whether the
robot is powered, which camera app they prefer).

## What this project is (read first)

- `FruitFlyPrototype/FruitFlyHost/` — Python host. Runs the MaleCNS fly
  connectome (`flybrain`, numba CPU) and sends FORWARD/LEFT/RIGHT/ROTATE/STOP
  to the ESP32 over HTTP. Has a dashboard on port 8642.
- `FruitFlyPrototype/RobotPrototype/` — ESP32 firmware. **Already flashed.
  Do not modify or try to build it here.** It keeps final authority over
  obstacle stop, e-stop and motor GPIO; the phone only proposes intents.
- Docs to read: `FruitFlyPrototype/SETUP_TERMUX.md` (phone setup),
  `FruitFlyPrototype/FruitFlyHost/README.md` (flags, camera options),
  `FruitFlyPrototype/FruitFlyHost/FLYBRAIN_NOTES.md` (what the model is).

## Hard constraints

- Stay CPU: `FLY_DEVICE=cpu`. No CuPy, no CUDA, no PyTorch.
- No OpenCV. Camera comes in over HTTP (browser push page or IP Webcam).
  `opencv-python-headless` is optional and only for the laptop webcam.
- Do not change safety defaults (`Policy` thresholds, `stop_cm`), the
  firmware, or `vision.py` gains unless the user asks after seeing it run.
- Motors stay off (`--enable-motors` absent) until the user explicitly says
  the wheels are lifted or they accept it driving.
- Everything must work with **no internet** once the phone joins the
  robot's Wi-Fi AP (`RobotPrototype`, password in `RobotPrototype/config.h`).
  Download everything before that step.

## Step 1 — environment

```bash
uname -m                      # must print aarch64
python3 --version             # 3.10+ required
cd ~/RC/FruitFlyPrototype     # or wherever the repo was cloned
python3 -m venv .venv
. .venv/bin/activate
pip install --upgrade pip
pip install -r FruitFlyHost/requirements.txt
```

If `apt` is needed first: `apt update && apt install -y python3 python3-venv python3-pip build-essential git`.
If pip tries to *compile* numba/llvmlite instead of downloading a wheel,
stop and report: that means no aarch64 wheel matched (wrong Python or
32-bit userland). Do not attempt a source build.

Verify: `python -c "import flybrain, numba, numpy, scipy, PIL; print('ok')"`.

## Step 2 — brain files (needs internet, ~260 MB, once)

```bash
python -m flybrain download
flybrain info
```

Expected: `~/fly-data/brain.npz` and `weights.npz` exist, sha256 verified by
the tool. If the phone's storage is tight, set `FLY_DATA=/sdcard/fly-data`
and add `export FLY_DATA=/sdcard/fly-data` to `~/.bashrc`.

## Step 3 — smoke test without robot or camera

```bash
cd FruitFlyHost
chmod +x run.sh
./run.sh --sim-robot --quiet
```

`run.sh` sets `FLY_DEVICE=cpu`, `--hz 5`, `--steps-per-tick 1`. First run
compiles numba kernels (30–90 s on a phone). Success looks like:

```
brain device=cpu  robot=SIM  camera=none
dashboard    http://<ip>:8642/
```

Then, from another Termux session (or the phone browser):
`curl -s http://127.0.0.1:8642/api/state | head -c 300` returns JSON with
`"ok": true`, and `http://127.0.0.1:8642/` in the phone browser shows the
brain scatter moving. Record the `step_ms` value from `/api/state`; if it
is above ~150 ms tell the user and keep `--hz 5`.

Stop with Ctrl+C.

## Step 4 — camera on the same phone

Preferred (no app): run with `--camera push`, then open
`http://127.0.0.1:8642/camera` in Chrome on the phone and tap **Start
streaming**. `127.0.0.1` is a secure context so `getUserMedia` works.
Chrome must stay in the foreground or split-screen (Android pauses
background tabs).

Alternative: the **IP Webcam** app. Start it, then
`--camera http://127.0.0.1:8080/video`. Survives screen-off.

Verify: `/api/state` → `"camera": {"has_frame": true ...}` and the two eye
tiles on the dashboard show the picture.

## Step 5 — join the robot and dry-run

1. User connects the phone to Wi-Fi `RobotPrototype`. Android may complain
   the network has no internet and switch back to mobile data; the user
   must tap "keep connection" / disable auto-switch.
2. Check: `curl -s -m 3 http://192.168.4.1/api/status`. Must return JSON
   with `distance_cm`. If it times out, the phone is not on the AP; stop
   and tell the user.
3. In Termux (not inside Debian): `termux-wake-lock` so the CPU stays awake.
4. Dry run (robot only ever receives STOP + heartbeat):

```bash
./run.sh --camera push --quiet
```

Verify on the dashboard: robot **online**, range updating, decision verb
changing when a hand approaches the phone camera, "sent to robot: stop".

## Step 6 — motors (only when the user says so)

```bash
./run.sh --camera push --enable-motors --demo-forward --allow-turns
```

Behaviour to expect: forward at ~48/255 when clear; a looming object →
stop, then rotate away for ~0.8 s at 5 Hz ticks. The ESP32 still stops on
its own at ≤ 30 cm. The dashboard's **Emergency stop** button works from
the phone; **Clear stop** releases it.

## Troubleshooting

| Symptom | Do |
|---|---|
| `ModuleNotFoundError: flybrain` | venv not active: `. .venv/bin/activate` |
| `numba` compile hangs > 3 min | `export NUMBA_NUM_THREADS=4` and retry; report device CPU count |
| `/api/state` 404 or refused | bridge not running or crashed; read its console output |
| `camera: push, no frames` | camera page not streaming, or Chrome in background |
| `robot offline` on dashboard | phone not on `RobotPrototype` Wi-Fi, or robot unpowered |
| decision always `stop`, reason `range NN cm` | ultrasonic sees something < 30 cm; expected |
| `connection error ... sending no motion` in log | Wi-Fi dropped; the ESP32 halts itself after 1.5 s without heartbeat |

## Report back

When done, give the user: the `step_ms` on this phone, which camera route
worked, and the exact `./run.sh ...` command to use next time. Do not
commit or push anything from the phone unless asked.
