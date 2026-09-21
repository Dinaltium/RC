# Running the FruitFly host on the phone (Termux + Debian)

> Handing the phone to someone new? `PHONE_GUIDE.md` is the same procedure
> written out step by step with every command ready to copy. This file is
> the short version for people who already know Termux.

Goal: the phone is camera **and** brain. It joins the robot's Wi-Fi AP,
runs `fruitfly_bridge.py` inside a Debian proot, watches its own camera,
and sends intents to the ESP32 at `192.168.4.1`. The laptop is no longer
needed.

Everything in `FruitFlyHost/` was written for this: stdlib HTTP server, no
OpenCV, no PyTorch, CPU brain via numba, and every input reachable over
`127.0.0.1`.

## 0. Requirements

* Android phone, arm64, ≥ 4 GB RAM (the brain needs ~600 MB resident).
* Termux from the **GitHub releases** page
  (https://github.com/termux/termux-app/releases) or **F-Droid** — both are
  fine. Not the Play Store build, which is abandoned.
* ~1.5 GB free storage (Debian + Python packages + 260 MB brain files).
* Internet for the install steps only — once on the robot's AP the phone
  has no internet, so download everything first.

## 1. Termux

```bash
pkg update && pkg upgrade -y
pkg install -y proot-distro termux-api wget
proot-distro install debian
termux-setup-storage          # allow access to /sdcard (optional)
```

## 2. Debian

```bash
proot-distro login debian
apt update && apt install -y python3 python3-venv python3-pip git build-essential
mkdir -p ~/rc && cd ~/rc
# copy the project in (any of): git clone, `adb push`, or from /sdcard:
#   cp -r /sdcard/RC/FruitFlyPrototype .
cd FruitFlyPrototype
python3 -m venv .venv
. .venv/bin/activate
pip install --upgrade pip
pip install -r FruitFlyHost/requirements.txt
```

`numba`, `llvmlite`, `scipy`, `numpy` and `pillow` all ship aarch64
manylinux wheels, so no compilation is needed. If pip tries to build
llvmlite from source your Debian is 32-bit or the wheel index is blocked —
check `uname -m` prints `aarch64`.

## 3. Brain files

```bash
python -m flybrain download      # 260 MB into ~/fly-data (inside the proot)
flybrain info
```

To keep the files on shared storage instead: `export FLY_DATA=/sdcard/fly-data`
(add it to `~/.bashrc`).

## 4. Smoke test (no robot, no camera)

```bash
cd ~/rc/FruitFlyPrototype/FruitFlyHost
./run.sh --sim-robot
```

First run compiles numba kernels (30–90 s on a phone). Then open
`http://127.0.0.1:8642/` in the phone's browser: you should see the
brain scatter flashing and the escape bars react to the simulated range
sweep. Expect 40–150 ms per brain step on a phone CPU, which is why
`run.sh` defaults to `--hz 5 --steps-per-tick 1`.

## 5. Camera on the same phone

Either:

* **Browser page** — open `http://127.0.0.1:8642/camera` in Chrome, tap
  *Start streaming*. `127.0.0.1` is a secure context, so `getUserMedia`
  works without flags. Run the bridge with `--camera push`. Keep Chrome in
  the foreground or in split-screen; Android pauses background tabs.
* **IP Webcam app** — start it, then `--camera http://127.0.0.1:8080/video`.
  Survives screen-off with the app's own wake lock.

## 6. Robot

1. Join the phone to Wi-Fi `RobotPrototype` (password in
   `RobotPrototype/config.h`). Disable "switch to mobile data when Wi-Fi
   has no internet" or the phone will drop the AP.
2. `termux-wake-lock` in Termux so the CPU stays awake.
3. Dry run, then bench:

```bash
./run.sh --camera push                                   # dry-run
./run.sh --camera push --enable-motors --demo-forward --allow-turns
```

The dashboard's **E-STOP** button and the robot's own web UI
(`http://192.168.4.1/`) both still work from the phone. To drive by hand,
open `http://127.0.0.1:8642/controls`, switch to *Manual* and hold the
pad; switch back to *Fly brain* to let the connectome drive. If the robot
carries the ESP32-CAM, `--camera http://192.168.4.20/stream` replaces the
phone camera.

## 7. Known constraints

| Constraint | Handling |
|---|---|
| Single CPU brain step is slow | `--hz 5 --steps-per-tick 1`; the decoder uses EMAs, so lower rates still work |
| Thermal throttling | Expect step time to climb after a few minutes; the dashboard shows `step ms` |
| No internet on the AP | Everything is local; `flybrain download` must be done beforehand |
| Android kills background apps | `termux-wake-lock`, keep Termux notification, keep the camera app foreground |
| proot has no GPU | `FLY_DEVICE=cpu` is forced by `run.sh` |
| Battery | Phone drives camera + Wi-Fi + CPU at 100 %; plan on a power bank for long sessions |

## 8. What is deliberately unchanged

* Firmware: identical to the laptop phase. The phone talks to the same
  `/api/command` and `/api/status` endpoints.
* Safety: the ESP32 still stops at ≤ 30 cm / LD2420 presence, honours its
  latched e-stop and halts on a 1.5 s heartbeat gap. Nothing on the phone
  can bypass it.
