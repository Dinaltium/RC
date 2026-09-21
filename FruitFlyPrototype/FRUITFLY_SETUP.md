# Fruit-fly experiment setup

This folder is a copy of the working ESP32 robot prototype. The original
project is unchanged.

## Important architecture

Do not try to put the full fruit-fly connectome on the ESP32. The MaleCNS
dataset is a research-scale wiring diagram with more than 166,000 neurons.
Run the experimental model on a laptop, desktop, or Raspberry Pi, and send
only high-level intents to the ESP32:

```text
camera / ultrasonic / IMU
          -> host-side fruit-fly experiment
          -> FORWARD, BACKWARD, LEFT, RIGHT, STOP + speed
          -> existing ESP32 command parser
          -> control manager -> safety manager -> motors
```

The ESP32 must remain the final authority for obstacle stopping, emergency
stop, command timeouts, and motor GPIO control.

## 1. Create a host environment

From PowerShell:

```powershell
cd C:\Projects\RC\FruitFlyPrototype
py -3 -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install flybrain
```

If PowerShell blocks activation, use the Python executable directly:

```powershell
.\.venv\Scripts\python.exe -m pip install flybrain
```

## 2. Download the released brain model

Google released the MaleCNS connectome/dataset rather than a single trained
model. The most direct runnable Python package I found for it is `flybrain`,
which simulates the wiring diagram as a leaky integrate-and-fire network.

```powershell
python -m flybrain download
flybrain info
```

The prebuilt download is roughly 260 MB. To build it directly from the
released MaleCNS files instead:

```powershell
pip install "flybrain[build]"
python -m flybrain build
```

The source build is roughly 1.1 GB. Set `FLY_DATA` if you want the cache in a
specific location. Official references:

- https://research.google/blog/a-connectomics-milestone-mapping-the-complete-male-fruit-fly-brain/
- https://sites.research.google/gr/neural-mapping/datasets/

Runnable implementation:

- https://github.com/alextitonis/fly.ai

Smoke-test a known visual circuit:

```powershell
python -c "from flybrain import FlyBrain; b=FlyBrain(device='auto'); b.stimulate(b.cells(['LC4','LPLC2'], side='L'), 0.8); print(len(b.step()))"
```

Then map activity from descending neurons to the robot's four movement
intents. The full brain runs on the host; it does not belong on the ESP32.

## 3. Arduino firmware

The firmware lives in:

```text
FruitFlyPrototype\RobotPrototype        <- flash this one
FruitFlyPrototype\ESP32CAMFallback      <- not used for now (see below)
```

Compile and upload `RobotPrototype` exactly as the original prototype. The
host-side model communicates through the existing Wi-Fi command interface;
it never gets direct motor-pin access.

Differences from the original `C:\Projects\RC\RobotPrototype`:

* **2-motor build**: FL/FR only, both DRV8833 bridges paralleled per motor,
  GPIO23 drives the DRV8833 `EEP` enable (see README §3).
* **Front range sensor selectable** in `config.h`: `FRONT_RANGE_HCSR04`
  (default), `FRONT_RANGE_LD2420` presence radar, or `FRONT_RANGE_VL53L0X`.
* **Bump endstops** (GPIO 34/35) and **VL53L0X cliff sensors** (I²C 21/22,
  XSHUT 32/33) feed the safety manager; see `WIRING.md`. Needs the Pololu
  `VL53L0X` library.
* **ESP32-CAM / ESP-NOW disabled**: `ENABLE_ESPNOW` is commented out in
  `config.h`. The receiver is compiled out and Wi-Fi runs in plain AP mode.
  The phone is the camera instead (see `FruitFlyHost/README.md`). Uncomment
  the define to bring the CAM back.
* Bug fixes shared with the original: interrupt-driven ultrasonic echo
  (loop no longer blocks on `pulseIn`), ESP-NOW command byte range-checked
  before the enum cast, memory barriers on the ESP-NOW ring buffer, dead
  `parseEspNowPacket()` (trusted a spoofable source byte) removed, unused
  motor slots use an explicit `PIN_UNUSED` sentinel instead of GPIO 0.

## 4. Host, camera, dashboard

See `FruitFlyHost/README.md` (laptop), `FruitFlyHost/FLYBRAIN_NOTES.md`
(what the model is and how to improve it) and `SETUP_TERMUX.md` (moving the
host onto the phone).

## Implementation order

1. [done] Firmware compiles; safety path unchanged.
2. [done] `--sim-robot` exercises brain + dashboard with no hardware.
3. [done] Bridge speaks the existing `/api/command` + `/api/status` protocol.
4. [done] Phone camera → retina → LPLC2/LC4/LC10a → DNp01/DNa02 readouts.
5. [next] Bench test with wheels lifted, then floor test; tune `vision.py`
   gains with `calibrate.py` and real footage.
6. Move the host to the phone (`SETUP_TERMUX.md`).
7. Trained readouts / better steering (`FLYBRAIN_NOTES.md` §5).

This keeps the fruit-fly work reversible and prevents an experimental model
from bypassing the robot's safety path.
