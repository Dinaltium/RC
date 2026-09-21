"""FruitFly controller: connectome in, robot intent out.

    camera + range  -> Retina (vision.py) -> LPLC2 / LC4 / LC10a
                    -> FlyBrain.step() x N (the MaleCNS connectome, untrained)
                    -> descending-neuron readouts:
                         DNp01  escape        (brain.groups["escape_L/R"])
                         DNa02  steering      (brain.groups["steer_L/R"])
                         DNg100 forward walk  (brain.groups["forward_L/R"])
                         MDN    backward walk (brain.groups["backward_L/R"])
                    -> decoder -> FORWARD / LEFT / RIGHT / ROTATE_* / BACKWARD / STOP

The decoder is a small saccade state machine, modelled on how flies (and the
optic-flow robots built after them) actually avoid things: a *trigger* (the
looming pathway, or the range sensor) starts one open-loop manoeuvre that is
committed to until the way ahead is clear, then a refractory pause. Three
rules keep it from the forward / turn / forward / turn loop:

  1. turn until clear, not for a fixed time: the rotation ends when the
     range sensor reads past `clear_cm` *and* the eyes see no loom, with a
     minimum and maximum length;
  2. direction memory: escapes that follow each other within `memory_s`
     keep the same turning direction, so the robot does not ping-pong
     between two walls;
  3. escalation: the third escape inside `memory_s` backs up first and turns
     roughly twice as far, i.e. it gives up on that heading.

Movement other than STOP needs --enable-motors; forward needs --demo-forward;
turns need --allow-turns. The ESP32 still enforces obstacle stop, bumpers,
e-stop and timeouts underneath.
"""
from __future__ import annotations

import random
import time
from dataclasses import dataclass

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
    max_speed: int = 200              # PWM 0..255 for cruising and turning
    min_speed: int = 120              # brushed BO motors stall below ~110 on 2S; never send less
    escape_threshold: float = 0.2     # EMA spikes/step on DNp01 (ceiling 0.5); two spikes within ~3 ticks
    steer_threshold: float = 0.2      # |L-R| EMA on DNa02: one spike gives 0.15, two close together ~0.25
    stop_cm: float = 30.0             # at or under this the range sensor triggers an avoid (firmware blocks forward here too)
    clear_cm: float = 45.0            # a turn may end once the range reads at least this
    freeze_s: float = 0.2             # STOP before the manoeuvre (the fly's freeze)
    reverse_s: float = 0.7            # back up this long when the range sensor (not the eyes) triggered
    turn_min_s: float = 0.6           # a saccade is at least this long ...
    turn_max_s: float = 4.0           # ... and gives up after this long even if still blocked
    refractory_s: float = 0.8         # ignore DNp01 for this long after a manoeuvre
    memory_s: float = 8.0             # escapes inside this window share a direction
    escalate_after: int = 3           # n-th escape inside memory_s backs up and turns ~2x
    loom_clear: float = 0.3           # retina loom drive must be under this for "clear"


class FruitFlyController:
    def __init__(self, brain, policy: Policy, steps_per_tick: int = 2,
                 ema: float = 0.8, encoder: dict | None = None, hz: float = 10.0):
        self.brain = brain
        self.policy = policy
        self.hz = hz
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
        self.step_ms = 0.0

        # manoeuvre state
        self.state = "cruise"            # cruise | freeze | reverse | turn | refractory
        self.ticks_left = 0              # countdown inside freeze / reverse / refractory
        self.turn_ticks = 0              # how long the current turn has run
        self.turn_min = 0
        self.turn_max = 0
        self.reverse_planned = False
        self.reverse_ticks = 0
        self.turn_action = "ROTATE_LEFT"
        self.escape_times: list[float] = []   # monotonic timestamps of recent escapes
        self.escapes_total = 0
        self.last_trigger = ""
        self.rng = random.Random(7)

    def _ticks(self, seconds: float) -> int:
        return max(1, int(round(seconds * self.hz)))

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
    def _cruise_speed(self, status) -> int:
        p = self.policy
        drive = self.rates["forward_L"] + self.rates["forward_R"]
        speed = p.max_speed * (0.75 + drive)
        # ease off as the range closes so the stop is not a crash
        if status.distance_cm < 2 * p.stop_cm:
            speed *= 0.7
        return int(np.clip(speed, p.min_speed, p.max_speed))

    def _turn_speed(self) -> int:
        return max(self.policy.min_speed, self.policy.max_speed)

    def _range_blocked(self, status) -> bool:
        return status.obstacle or status.distance_cm <= self.policy.stop_cm

    def _way_clear(self, status) -> bool:
        loom = max(self.retina.last.get("loomL", 0.0), self.retina.last.get("loomR", 0.0))
        return (not status.obstacle and status.distance_cm >= self.policy.clear_cm
                and loom < self.policy.loom_clear)

    def _start_avoid(self, trigger: str, threat_left: bool | None, status) -> None:
        """Plan one manoeuvre. trigger: 'range' | 'eyes' | 'bump'."""
        p = self.policy
        now = time.monotonic()
        self.escape_times = [t for t in self.escape_times if now - t <= p.memory_s]
        recent = len(self.escape_times)
        self.escape_times.append(now)
        self.escapes_total += 1

        # direction: recent escapes keep their direction (no ping-pong);
        # otherwise turn away from the threat; otherwise away from the
        # fuller eye; otherwise coin flip.
        if recent > 0:
            action = self.turn_action
        elif threat_left is not None:
            action = "ROTATE_RIGHT" if threat_left else "ROTATE_LEFT"
        else:
            sz = self.retina.state.size
            l, r = sz.get("L", 0.0), sz.get("R", 0.0)
            if abs(l - r) > 0.02:
                action = "ROTATE_RIGHT" if l > r else "ROTATE_LEFT"
            else:
                action = self.rng.choice(("ROTATE_LEFT", "ROTATE_RIGHT"))
        self.turn_action = action

        escalate = recent + 1 >= p.escalate_after
        if escalate:
            self.escape_times.clear()       # start counting again after the big one
        self.reverse_planned = trigger in ("range", "bump") or escalate
        self.reverse_ticks = self._ticks(p.reverse_s * (2 if escalate else 1))
        self.turn_min = self._ticks(p.turn_min_s * (2.2 if escalate else 1))
        self.turn_max = self._ticks(p.turn_max_s)
        self.turn_ticks = 0
        self.last_trigger = trigger + (" (escalated)" if escalate else "")
        self.state = "freeze"
        self.ticks_left = self._ticks(p.freeze_s)

    def _decide(self, status) -> Decision:
        p = self.policy
        r = self.rates
        if status.emergency_stop:
            self.state = "cruise"
            return Decision("STOP", 0, "robot e-stop latched")
        if status.fault:
            self.state = "cruise"
            return Decision("STOP", 0, "robot fault")
        if status.cliff_front or status.cliff_rear:
            self.state = "cruise"
            return Decision("STOP", 0, "cliff " + ("ahead" if status.cliff_front else "behind") + ", latched on robot")
        if status.bump_rear and self.state == "reverse":
            self.state = "turn"            # cannot back up any further; turn in place
        if status.bump_front and self.state in ("cruise", "refractory"):
            self._start_avoid("bump", None, status)
        elif status.bump_front and self.state == "turn":
            # the firmware refuses rotation while the front bumper is pressed;
            # back off first, then resume the same turn
            self.state = "reverse"
            self.ticks_left = self.reverse_ticks

        # --- inside a manoeuvre -------------------------------------------
        if self.state == "freeze":
            self.ticks_left -= 1
            if self.ticks_left <= 0:
                self.state = "reverse" if self.reverse_planned else "turn"
                self.ticks_left = self.reverse_ticks
            return Decision("STOP", 0, f"freeze, {self.last_trigger}")

        if self.state == "reverse":
            if status.bump_rear or status.cliff_rear:
                self.state = "turn"
            else:
                self.ticks_left -= 1
                if self.ticks_left <= 0:
                    self.state = "turn"
                if p.demo_forward:
                    return Decision("BACKWARD", p.min_speed, f"backing away, {self.last_trigger}")
                return Decision("STOP", 0, "would back away (forward disabled)")

        if self.state == "turn":
            self.turn_ticks += 1
            done = self.turn_ticks >= self.turn_max or (self.turn_ticks >= self.turn_min and self._way_clear(status))
            if done:
                self.state = "refractory"
                self.ticks_left = self._ticks(p.refractory_s)
                why = "way clear" if self.turn_ticks < self.turn_max else "gave up turning"
                return Decision("STOP", 0, f"{why} after {self.turn_ticks / self.hz:.1f} s")
            if p.allow_turns:
                return Decision(self.turn_action, self._turn_speed(),
                                f"{self.turn_action.lower().replace('_', ' ')} until clear, range {status.distance_cm:.0f} cm")
            return Decision("STOP", 0, "would turn (turns disabled)")

        if self.state == "refractory":
            self.ticks_left -= 1
            if self.ticks_left <= 0:
                self.state = "cruise"

        # --- cruising: what starts a manoeuvre? -----------------------------
        if self._range_blocked(status):
            self._start_avoid("range", None, status)
            return Decision("STOP", 0, f"range {status.distance_cm:.0f} cm -> back up and turn")

        if self.state == "cruise":
            esc_l, esc_r = r["escape_L"], r["escape_R"]
            if max(esc_l, esc_r) >= p.escape_threshold:
                threat_left = esc_l > esc_r if abs(esc_l - esc_r) >= 0.05 else None
                self._start_avoid("eyes", threat_left, status)
                return Decision("STOP", 0, f"DNp01 escape L={esc_l:.2f} R={esc_r:.2f} -> {self.turn_action.lower().replace('_', ' ')}")

        steer = r["steer_L"] - r["steer_R"]
        if p.allow_turns and abs(steer) >= p.steer_threshold:
            # DNa02 is ipsilateral: left DNa02 -> turn left.
            action = "LEFT" if steer > 0 else "RIGHT"
            return Decision(action, self._cruise_speed(status), f"DNa02 steer {steer:+.2f}")

        if p.demo_forward:
            drive = r["forward_L"] + r["forward_R"]
            return Decision("FORWARD", self._cruise_speed(status), f"clear, DNg100 {drive:.2f}")

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
            "state": self.state,
            "turn_action": self.turn_action,
            "escapes_recent": len(self.escape_times),
            "escapes_total": self.escapes_total,
        }
