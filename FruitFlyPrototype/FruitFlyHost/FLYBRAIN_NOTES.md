# FruitFly brain — what we are running and how to push it further

## 1. What Google actually released

Google did **not** release a trained fly-brain model. Together with HHMI
Janelia (FlyEM) and the Cambridge Connectomics Group they released
**MaleCNS v1.0**: the first complete connectome (wiring diagram) of an adult
male *Drosophila* central nervous system — brain, optic lobes and ventral
nerve cord, ~166,700 neurons, ~125 M synapses, with cell types, sides,
neurotransmitter predictions and neuron positions. Data went public in
June 2026 (CC-BY) and the paper appeared in *Cell* on 3 Sep 2026.

* Dataset & download: <https://male-cns.janelia.org/download/>
* Janelia project page: <https://www.janelia.org/project-team/flyem/male-cns-connectome>
* Query API (neuPrint): <https://neuprint.janelia.org>
* Google blog: <https://research.google/blog/a-connectomics-milestone-mapping-the-complete-male-fruit-fly-brain/>
* R access (natverse): <https://natverse.org/malecns/>

## 2. What `flybrain` adds (the package we run)

`flybrain` 0.1.0 (MIT, <https://github.com/alextitonis/fly.ai>) turns the
connectome into a **leaky integrate-and-fire spiking network**:

```text
v <- exp(-dt/tau) v + gain · W @ spikes + tonic + noise (+ injected)
v >= 1 -> spike, v = 0            dt = 20 ms, tau = 100 ms
```

Nothing is trained. `W` is the signed, normalized connectivity from
MaleCNS (sign from transmitter prediction). `tonic 0.14`, `gain 3.0` are the
author's calibration that keeps descending neurons quiet at rest while the
visual pathways still get through. `sensory_input=False` cuts synapses onto
sensory neurons (otherwise olfactory receptor neurons run away).

Files in `~/fly-data`: `weights.npz` (CSR, 25.6 M entries) and `brain.npz`
(`cell_type`, `side`, `superclass`, `positions`, eye layout, and readout
groups `group_forward/steer/escape/backward/punch/kick _L/_R`).

Runtime on this laptop: ~1.4 ms/step on the GPU (CuPy), ~10–16 ms/step on
CPU (numba). ~7,800 neurons spike per step at rest.

## 3. Findings from `fly.ai` we rely on

| Finding | Consequence for us |
|---|---|
| Photoreceptor → lamina relay does not carry signal in a spiking model (real lamina cells are graded) | We don't feed pixels to the 6,006 photoreceptors; we drive **visual projection neurons** directly (`vision.py`) |
| LC4 + LPLC2 (left) → **DNp01** giant fiber (left), +17–25 spikes/s, ipsilateral | Escape readout = `group_escape_L/R`; a looming object on the left fires the left giant fiber |
| LC10a (left) → **DNa02** (left) steering, only +1.4–3.7 spikes/s | Steering signal is weak; needs a low threshold (0.04) and more work (see §5) |
| DNg100 = forward walking, MDN = backward walking | Exposed as `forward`/`backward` readouts; with our stimuli they stay near 0 |
| Default tonic parks neurons at 0.78 V | Any injection > 0.22 V fires a cell every step → graded *voltage* is useless; we use a **population code** (fraction of the cell type driven) |

## 4. Our measurements (`calibrate.py`, seed 64, 150 steps)

```text
condition                  escape_L   escape_R    steer_L    steer_R
rest                          0.000      0.013      0.013      0.007
loom L 0.4                    0.233      0.027      0.027      0.000
loom L 0.8                    0.407      0.013      0.013      0.007
threat L 0.8 (LC4)            0.500      0.013      0.020      0.000
loom+threat L 0.8             0.900      0.027      0.000      0.020
loom+threat both 0.8          0.940      0.907      0.007      0.013
chase L 0.6 (LC10a)           0.007      0.007      0.067      0.000
chase R 0.6 (LC10a)           0.007      0.000      0.000      0.060
```

* No reverberation: after 300 steps of bilateral drive the readouts return
  to rest within one 50-step window.
* With population coding, DNp01 responds gradedly to how much of LC4/LPLC2 is
  driven, and saturates at 0.5 spikes/step (it needs one step to recharge).
* A synthetic dark disc growing in the **left** half of the camera frame
  drives left DNp01 to 0.3–0.4 while the right stays < 0.15 → decoder emits
  STOP then ROTATE_RIGHT (away). Lateralization works end-to-end.

## 5. Improvement roadmap

Ordered by value / effort.

1. **Better looming estimate.** `size` is "pixels darker than the median";
   a dark floor or shadow can fool it. Options: background-normalized
   contrast, blob tracking (largest connected dark region per eye), or
   optical-flow divergence (expanding flow field = looming). All doable with
   numpy only.
2. **Steering via the fly's own pathways.** DNa02 is weak under LC10a
   alone. Try adding `LC10b/c/d`, `LC9`, `LPLC4`, or driving the central
   complex heading cells. Use `calibrate.py` conditions to check each.
3. **Train a readout instead of hand rules.** `flybrain.reservoir.Trace` +
   `Readout.fit` learns a linear map from descending-neuron traces to
   labels. Record (frame, range, human command) sessions with the web UI,
   then fit `Readout` to predict LEFT/RIGHT/FORWARD/STOP from all 1,314
   descending neurons. This is how fly.ai's `sshfighter` works.
4. **Use `positions` for a proper 3-D view** (Three.js in the dashboard) —
   the 2-D projection already shows brain vs VNC.
5. **Batched flies.** `FlyBrain(batch=8)` runs 8 brains for ~the cost of
   1–2 on GPU; vote across them to de-noise decisions, or run 8 encoder
   parameter sets side by side for tuning.
6. **Refractory period** (`refractory=0.005`) to make rates more
   biological; re-run `calibrate.py` afterwards.
7. **Replace the LD2420 presence radar** with a real range sensor (HC-SR04
   or VL53L1X ToF) so `range_threat` is graded rather than 0/999 cm.
8. **Ask the connectome directly.** neuPrint lets you list which visual
   projection types synapse onto DNp01/DNa02/DNg100; use that to pick the
   encoder channels rather than guessing.

## 6. Honest limits

Point neurons, one sign per neuron, no dendrites, no plasticity, no
neuromodulation, hand-set tonic/gain, a shortcut visual front end, and no
validation against real recordings. It is a research toy on top of a real
wiring diagram — which is exactly why the ESP32 keeps the last word.
