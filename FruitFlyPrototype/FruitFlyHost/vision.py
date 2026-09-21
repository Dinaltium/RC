"""Retina: turns a camera frame (+ the range sensor) into drive for the fly's
own visual projection neurons.

Why not feed pixels to the photoreceptors? flybrain's authors found the
photoreceptor -> lamina relay does not carry signal in a spiking model (the
real neurons are graded). The route that works is to drive the identified
feature-detector types directly, on the side where the thing is:

    LPLC2  looming (angular growth of an object)      -> DNp01 escape
    LC4    fast looming / imminent collision           -> DNp01/02/04 escape
    LC10a  a target worth tracking                     -> DNa02 steering
    LC11   small moving object                         (motion, not used for control)

The encoder below is deliberately simple and hand-tuned. It is *a model* of
the fly's eye; everything downstream of these neurons is the connectome.

Per eye (left / right half of the frame) we compute:
    size      fraction of the half that is "object": pixels that differ from
              a slowly learned background (bright or dark), or are much
              darker than the scene
    growth    increase of size since the last frame (looming)
    motion    mean absolute frame difference (moving stuff)
    open      brightness of the lower half (free floor ahead)

The range sensor is fused as a "threat" so the fly still flinches when the
camera is off or dark.
"""
from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

# Population code. The brain's tonic input parks every neuron at ~0.78 V
# (threshold 1.0), so any voltage above ~0.22 fires a cell on every step:
# graded *voltage* is nearly binary. Instead each channel's intensity (0..1)
# selects the FRACTION of that cell type that receives `volt`, like an object
# covering part of the eye. Rates downstream then scale with intensity.
ENCODER = {
    "loom_gain": 20.0,      # growth (fraction/frame) -> LPLC2 intensity
    "loom_size": 1.2,       # size (fraction) -> LPLC2, looming cells also like big things
    "threat_gain": 1.0,     # (range-derived + collision) -> LC4 intensity
    "collision_size": 0.30, # size above this counts as "about to hit"
    "chase_gain": 0.5,      # open-floor asymmetry -> LC10a (steer toward open side)
    "motion_gain": 2.0,     # motion -> LC11 (display only by default)
    "volt": 0.5,            # voltage given to the selected cells
    "min_frac": 0.03,       # below this intensity nothing is driven
    "range_near_cm": 30.0,  # ultrasonic: full threat here
    "range_far_cm": 60.0,   # ...and none here
    "bg_rate": 0.03,        # background learning rate per frame (~3 s to absorb a still object)
    "fg_thresh": 0.13,      # |frame - background| above this = object
    "dark_thresh": 0.18,    # below (median - this) = object even if in the background
}

CHANNELS = {
    "loom": ["LPLC2"],
    "threat": ["LC4"],
    "chase": ["LC10a"],
    "motion": ["LC11"],
}


@dataclass
class RetinaState:
    """Per-eye features from the last frame, for the dashboard."""
    size: dict = field(default_factory=lambda: {"L": 0.0, "R": 0.0})
    growth: dict = field(default_factory=lambda: {"L": 0.0, "R": 0.0})
    motion: dict = field(default_factory=lambda: {"L": 0.0, "R": 0.0})
    open: dict = field(default_factory=lambda: {"L": 0.0, "R": 0.0})
    range_threat: float = 0.0
    has_frame: bool = False


class Retina:
    def __init__(self, brain, **encoder):
        unknown = set(encoder) - set(ENCODER)
        if unknown:
            raise ValueError(f"unknown encoder parameters: {sorted(unknown)}")
        self.p = {**ENCODER, **encoder}
        self.cells = {ch: {s: np.sort(brain.cells(types, s)) for s in "LR"} for ch, types in CHANNELS.items()}
        self.prev_gray: np.ndarray | None = None
        self.background: np.ndarray | None = None
        self.mask: np.ndarray | None = None          # last object mask, for the dashboard
        self.prev_size = {"L": 0.0, "R": 0.0}
        self.state = RetinaState()
        # drive amounts from the last call, keyed "loomL", "threatR", ...
        self.last: dict[str, float] = {f"{ch}{s}": 0.0 for ch in CHANNELS for s in "LR"}

    # ------------------------------------------------------------------
    def range_threat(self, distance_cm: float, obstacle: bool) -> float:
        p = self.p
        if obstacle or distance_cm <= p["range_near_cm"]:
            return 1.0
        if distance_cm >= p["range_far_cm"]:
            return 0.0
        return float((p["range_far_cm"] - distance_cm) / (p["range_far_cm"] - p["range_near_cm"]))

    def _features(self, gray: np.ndarray) -> dict[str, dict[str, float]]:
        h, w = gray.shape
        half = w // 2
        p = self.p
        # "Object" = differs from the slowly learned background (a hand, a
        # person, a box that arrived), or is much darker than the scene.
        if self.background is None:
            self.background = gray.copy()
        fg = np.abs(gray - self.background) > p["fg_thresh"]
        dark = gray < max(float(np.median(gray)) - p["dark_thresh"], 0.08)
        obj = fg | dark
        # Learn the background only where nothing is detected, so a still
        # object fades in slowly rather than being absorbed instantly.
        rate = np.where(obj, p["bg_rate"] * 0.25, p["bg_rate"]).astype(np.float32)
        self.background += rate * (gray - self.background)
        self.mask = obj
        diff = np.abs(gray - self.prev_gray) if self.prev_gray is not None else np.zeros_like(gray)
        lower = gray[h // 2:, :]
        feats: dict[str, dict[str, float]] = {"size": {}, "growth": {}, "motion": {}, "open": {}}
        for side, sl in (("L", slice(0, half)), ("R", slice(half, w))):
            size = float(obj[:, sl].mean())
            feats["size"][side] = size
            feats["growth"][side] = max(0.0, size - self.prev_size[side])
            feats["motion"][side] = float(diff[:, sl].mean())
            feats["open"][side] = float(lower[:, sl].mean())
            self.prev_size[side] = size
        self.prev_gray = gray
        return feats

    # ------------------------------------------------------------------
    def inject(self, gray: np.ndarray | None, distance_cm: float, obstacle: bool) -> list:
        """Build the (neuron indices, voltage) list for brain.step(inject=...)."""
        p = self.p
        drive = {key: 0.0 for key in self.last}
        rt = self.range_threat(distance_cm, obstacle)
        st = RetinaState(range_threat=rt, has_frame=gray is not None)

        if gray is not None:
            f = self._features(gray)
            st.size, st.growth, st.motion, st.open = f["size"], f["growth"], f["motion"], f["open"]
            for s in "LR":
                drive[f"loom{s}"] = min(1.0, f["growth"][s] * p["loom_gain"] + f["size"][s] * p["loom_size"])
                collision = 1.0 if f["size"][s] > p["collision_size"] else 0.0
                drive[f"threat{s}"] = min(1.0, max(rt, collision) * p["threat_gain"])
                drive[f"motion{s}"] = min(1.0, f["motion"][s] * p["motion_gain"])
            # Steer toward the more open side: LC10a on that side.
            gap = f["open"]["L"] - f["open"]["R"]
            if abs(gap) > 0.05:
                s = "L" if gap > 0 else "R"
                drive[f"chase{s}"] = min(1.0, abs(gap) * p["chase_gain"] * 4.0)
        else:
            # No camera: range sensor alone, symmetric.
            for s in "LR":
                drive[f"threat{s}"] = min(1.0, rt * p["threat_gain"])
                drive[f"loom{s}"] = min(1.0, rt * 0.5)

        self.state = st
        self.last = drive
        inject = []
        for key, frac in drive.items():
            if frac < p["min_frac"]:
                continue
            cells = self.cells[key[:-1]][key[-1]]
            k = max(1, int(round(frac * len(cells))))
            inject.append((cells[:k], float(p["volt"])))
        return inject
