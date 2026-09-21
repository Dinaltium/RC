"""FruitFly controller: connectome in, robot intent out.

    camera + range  -> Retina (vision.py) -> LPLC2 / LC4 / LC10a
                    -> FlyBrain.step() x N (the MaleCNS connectome, untrained)
                    -> descending-neuron readouts:
                         DNp01  escape        (brain.groups["escape_L/R"])
                         DNa02  steering      (brain.groups["steer_L/R"])
                         DNg100 forward walk  (brain.groups["forward_L/R"])
                         MDN    backward walk (brain.groups["backward_L/R"])
                    -> decoder -> FORWARD / LEFT / RIGHT / ROTATE_* / BACKWARD / STOP

The decoder is intentionally conservative. Movement other than STOP needs
--enable-motors; forward needs --demo-forward; turns need --allow-turns.
The ESP32 still enforces obstacle stop, e-stop and timeouts underneath.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field

import numpy as np

from vision import Retina

READOUTS = ("escape", "steer", "forward", "backward")
WATCH_TYPES = ["LPLC2", "LC4", "LC10a", "LC11", "DNp01", "DNa02", "DNg100", "MDN"]


@dataclass
class Decision:
    action: str
    speed: int
    reason: str


@dataclass
class Policy:
    enable_motors: bool = False
    demo_forward: bool = False
    allow_turns: bool = False
    max_speed: int = 80
    escape_threshold: float = 0.2     # EMA spikes/step on DNp01 (ceiling 0.5); two spikes within ~3 ticks
    steer_threshold: float = 0.2      # |L-R| EMA on DNa02: one spike gives 0.15, two close together ~0.25
    turn_hold_ticks: int = 8          # keep turning away for this many ticks (0.8 s at 10 Hz)
    freeze_ticks: int = 1             # STOP ticks before the turn starts (the fly's freeze)
    refractory_ticks: int = 4         # after a manoeuvre, ignore DNp01 for this long
    stop_cm: float = 30.0


class FruitFlyController:
    def __init__(self, brain, policy: Policy, steps_per_tick: int = 2,
                 ema: float = 0.8, encoder: dict | None = None):
        self.brain = brain
        self.policy = policy
        self.steps_per_tick = max(1, int(steps_per_tick))
        self.ema = ema
        self.retina = Retina(brain, **(encoder or {}))

        g = brain.groups
        self.groups = {f"{name}_{s}": set(map(int, g[f"{name}_{s}"])) for name in READOUTS for s in "LR"}
        self.rates = {k: 0.0 for k in self.groups}      # EMA of spikes per step
        self.watch = {t: {s: set(map(int, brain.cells([t], s))) for s in "LR"} for t in WATCH_TYPES}
        self.watch_rates = {f"{t}{s}": 0.0 for t in WATCH_TYPES for s in "LR"}
        self.watch_counts = {k: 0 for k in self.watch_rates}   # raw spikes this tick (raster)
        self.group_counts = {k: 0 for k in self.groups}
        self.last_fired: np.ndarray = np.empty(0, np.int64)
        self.fired_total = 0
        self.turn_ticks_left = 0
        self.freeze_ticks_left = 0
        self.refractory_left = 0
        self.turn_action = "ROTATE_LEFT"
        self.step_ms = 0.0

    # ------------------------------------------------------------------
    def tick(self, gray, status) -> Decision:
        inject = self.retina.inject(gray, status.distance_cm, status.obstacle)
        started = time.perf_counter()
        counts = {k: 0 for k in self.groups}
        wcounts = {k: 0 for k in self.watch_rates}
        total = 0
        fired = np.empty(0, np.int64)
        for _ in range(self.steps_per_tick):
            fired = self.brain.step(inject=inject)
            fs = set(map(int, fired))
            total += len(fs)
            for k, members in self.groups.items():
                counts[k] += len(fs & members)
            for t, sides in self.watch.items():
                for s, members in sides.items():
                    wcounts[f"{t}{s}"] += len(fs & members)
        self.step_ms = (time.perf_counter() - started) * 1000.0 / self.steps_per_tick
        self.last_fired = fired
        self.fired_total = total
        self.watch_counts = wcounts
        self.group_counts = counts
        n = self.steps_per_tick
        for k in self.rates:
            self.rates[k] = self.ema * self.rates[k] + (1 - self.ema) * counts[k] / n
        for k in self.watch_rates:
            self.watch_rates[k] = self.ema * self.watch_rates[k] + (1 - self.ema) * wcounts[k] / n
        return self._decide(status)

    # ------------------------------------------------------------------
    def _decide(self, status) -> Decision:
        p = self.policy
        r = self.rates
        if status.emergency_stop:
            return Decision("STOP", 0, "robot e-stop latched")
        if status.fault:
            return Decision("STOP", 0, "robot fault")
        if status.cliff_front or status.cliff_rear:
            self.turn_ticks_left = 0
            return Decision("STOP", 0, "cliff " + ("ahead" if status.cliff_front else "behind") + ", latched on robot")
        if status.bump_front or status.bump_rear:
            self.turn_ticks_left = 0
            return Decision("STOP", 0, ("front" if status.bump_front else "rear") + " bumper pressed")
        if status.obstacle or status.distance_cm <= p.stop_cm:
            # Too close for the fly to matter: the ESP32 blocks forward anyway.
            # Rotating away is still allowed, so let a running turn continue.
            if self.turn_ticks_left > 0 and p.allow_turns:
                self.turn_ticks_left -= 1
                return Decision(self.turn_action, min(p.max_speed, 70), f"range {status.distance_cm:.0f} cm, turning away")
            self.freeze_ticks_left = 0
            return Decision("STOP", 0, f"range {status.distance_cm:.0f} cm")

        # --- escape manoeuvre state machine ---------------------------------
        # A looming object keeps DNp01 above threshold for many ticks. If we
        # re-triggered on every one of them the robot would only ever STOP.
        # So: trigger once, freeze briefly, then COMMIT to the turn for its
        # full duration, then ignore DNp01 for a short refractory period.
        if self.freeze_ticks_left > 0:
            self.freeze_ticks_left -= 1
            return Decision("STOP", 0, "escape freeze")
        if self.turn_ticks_left > 0:
            self.turn_ticks_left -= 1
            if self.turn_ticks_left == 0:
                self.refractory_left = p.refractory_ticks
            if p.allow_turns:
                return Decision(self.turn_action, min(p.max_speed, 70), "turning away from threat")
            return Decision("STOP", 0, "escape hold (turns disabled)")
        if self.refractory_left > 0:
            self.refractory_left -= 1
        else:
            esc_l, esc_r = r["escape_L"], r["escape_R"]
            if max(esc_l, esc_r) >= p.escape_threshold:
                # Which side is the threat on? DNp01 side first; if both fired
                # about equally, use which eye sees more object.
                sz = self.retina.state.size
                if abs(esc_l - esc_r) >= 0.05:
                    threat_left = esc_l > esc_r
                else:
                    threat_left = sz.get("L", 0.0) >= sz.get("R", 0.0)
                self.turn_action = "ROTATE_RIGHT" if threat_left else "ROTATE_LEFT"
                self.freeze_ticks_left = p.freeze_ticks
                self.turn_ticks_left = p.turn_hold_ticks
                return Decision("STOP", 0, f"DNp01 escape L={esc_l:.2f} R={esc_r:.2f} -> {self.turn_action.lower()}")

        steer = r["steer_L"] - r["steer_R"]
        if p.allow_turns and abs(steer) >= p.steer_threshold:
            # DNa02 is ipsilateral: left DNa02 -> turn left.
            action = "LEFT" if steer > 0 else "RIGHT"
            return Decision(action, p.max_speed, f"DNa02 steer {steer:+.2f}")

        if p.demo_forward:
            drive = r["forward_L"] + r["forward_R"]
            speed = int(np.clip(p.max_speed * (0.6 + drive), 40, p.max_speed))
            return Decision("FORWARD", speed, f"clear, DNg100 {drive:.2f}")

        return Decision("STOP", 0, "idle (demo-forward off)")

    # ------------------------------------------------------------------
    def snapshot(self) -> dict:
        return {
            "rates": {k: round(v, 3) for k, v in self.rates.items()},
            "watch": {k: round(v, 3) for k, v in self.watch_rates.items()},
            "watch_counts": self.watch_counts,
            "group_counts": self.group_counts,
            "steps": self.steps_per_tick,
            "drive": {k: round(v, 3) for k, v in self.retina.last.items()},
            "retina": {
                "size": self.retina.state.size,
                "growth": self.retina.state.growth,
                "motion": self.retina.state.motion,
                "open": self.retina.state.open,
                "range_threat": round(self.retina.state.range_threat, 3),
                "has_frame": self.retina.state.has_frame,
            },
            "fired_total": int(self.fired_total),
            "step_ms": round(self.step_ms, 2),
            "turn_ticks_left": self.turn_ticks_left,
        }
