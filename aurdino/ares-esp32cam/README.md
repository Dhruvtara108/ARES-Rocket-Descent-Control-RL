# ARES Rover: ESP32-CAM vision and navigation firmware

The ESP32-CAM captures grayscale frames, estimates where obstacles are with a
lightweight appearance-based heuristic, runs a deterministic navigation state
machine, and sends validated movement commands to the Arduino Nano over UART.
**The ESP32-CAM never drives the motor driver directly.**

## Current status (be precise)

| Part | Status |
|---|---|
| Protocol, image processing, obstacle detection, navigation | Implemented; **92 host unit checks pass** (synthetic frames, simulated inputs, 200,000-step safety fuzz) |
| Camera, UART and main loop (Arduino/ESP-IDF code) | Implemented; syntax-checked against stub headers only. **Not yet compiled for the ESP32 and not run on hardware.** |
| Nano side of the protocol | **Not implemented.** The Nano firmware is still the LED blink test. Required changes: [`docs/NANO_PROTOCOL.md`](docs/NANO_PROTOCOL.md) |
| Physical testing | **None performed.** |

Because the Nano does not answer yet, the link stays DOWN and the rover cannot
move. A second interlock, `cfg::UART_WIRING_CONFIRMED = false`, keeps the UART
off entirely until the wiring has been checked.

## Hardware assumptions to verify

| Item | Assumption | Where it comes from | Verify by |
|---|---|---|---|
| Board | AI-Thinker ESP32-CAM | Board photo shows the AI-Thinker header layout | Board marking; boot log |
| Camera | OV3660 (PID 0x3660) | Project brief | Boot log prints `Sensor identified` or a PID warning |
| Camera pins | Standard AI-Thinker map in `include/rover_config.h` | esp32-camera board definition | Camera initialises |
| UART pins | TX GPIO14, RX GPIO13 | **Proposed** (nothing in the repo) | Your wiring |
| Motor driver | Unknown (L298N or BTS7960) | Not in the repo | Nano side only |
| PSRAM | Present on most AI-Thinker boards | Not verified | Boot log `PSRAM yes/no` |

## Wiring

```
ESP32-CAM GPIO14 (TX, 3.3 V) ───────────────────────────► Nano RX pin (D7 proposed)
ESP32-CAM GPIO13 (RX)        ◄── 1 kΩ ──┬─────────────── Nano TX pin (D8 proposed, 5 V)
                                        2 kΩ
                                         │
ESP32-CAM GND ──────────────────────────┴─────────────── Nano GND  (common ground required)
ESP32-CAM 5V  ◄── regulated 5 V, at least 500 mA (separate buck converter recommended)
```

- The 1 kΩ / 2 kΩ divider brings the Nano's 5 V TX down to about 3.3 V. Never
  connect 5 V directly to an ESP32 pin. A logic-level shifter is also fine.
- 3.3 V from the ESP32 TX is read as HIGH by the 5 V Nano, so no shifter is
  needed in that direction.
- GPIO13/GPIO14 are microSD pins: **do not insert an SD card.**
- Do not use GPIO0, 2, 12, 15 (boot strapping), GPIO4 (flash LED), GPIO16
  (PSRAM) or GPIO1/3 (USB programming and logs).
- Do not power the ESP32-CAM from the Nano's 5 V pin or from the motor supply
  rail without a regulator; camera brown-outs reset the board.
- Fit a physical kill switch on motor power. Software stops are not enough.

## Build and flash

The ESP32-CAM has no USB port; use an ESP32-CAM-MB adapter or a USB-serial
adapter (TX→U0R, RX→U0T, GND, 5 V).

```bash
cd ares-esp32cam
pio run -e esp32cam                 # build
# Hold GPIO0 to GND (or press IO0 on the MB adapter), press RST, then:
pio run -e esp32cam -t upload
# Release GPIO0, press RST, then open the log:
pio device monitor -b 115200
```

Host unit tests (any PC with g++):

```bash
make -C ares-esp32cam/test/host
```

## Operator console (USB serial, 115200)

| Key | Action |
|---|---|
| `a` | Arm (required before any motion; refused while ESTOP is latched) |
| `d` | Disarm (STOP) |
| `e` | Emergency stop: latched, sends ESTOP frames to the Nano |
| `c` | Clear emergency stop (rover stays disarmed) |
| `s` | Status line |

## How it works

```
OV3660 160x120 GRAY ─► 80x60 box downsample ─► obstacle detector ─► navigator ─► NanoLink ─► UART ─► Nano ─► motor driver
     (frame buffer returned immediately)        (L/C/R scores)      (state machine)  (heartbeat, ACK)
```

**Camera** (`camera.cpp`): grayscale QQVGA so there is no JPEG decoding;
`CAMERA_GRAB_LATEST` with two PSRAM buffers when PSRAM exists. Every frame
buffer is returned before `grab()` exits. Five consecutive failures mark the
camera FAILED (navigator stops) and it is re-initialised every 3 s.

**Obstacle detection** (`obstacle_detection.cpp`): a heuristic, not object
recognition.

1. During the first 10 frames it learns a floor model (mean, std) from a patch
   at the bottom centre. **Start the rover facing clear, uniform floor.**
2. In rows 24 to 59, a pixel counts as obstacle-like if its brightness differs
   from the floor by more than max(18, 3 sigma); a strong Sobel edge counts half.
   Rows nearer the rover weigh up to twice as much.
3. The image is split into LEFT, CENTER and RIGHT thirds; each gets a score
   from 0 to 1.
4. Scores are smoothed (EMA 0.4) and a region switches to blocked above 0.30
   and back to clear below 0.18, each after 2 consecutive frames.
5. If the floor reference patch suddenly differs from the model, the CENTER
   is treated as blocked (something is right in front).
6. Too-dark or saturated images give UNCERTAIN, which stops the rover.

Confidence is a documented heuristic: how far the smoothed score is from the
switching threshold, 0 to 1. It is not a probability.

**Navigation** (`navigation.cpp`):

| Observation | Action |
|---|---|
| Path clear | FORWARD at speed 110, for at most 1.5 s, then a 0.3 s stop-and-look |
| Obstacle LEFT only | TURNING_RIGHT for 350 ms, stop, re-check |
| Obstacle RIGHT only | TURNING_LEFT for 350 ms, stop, re-check |
| Obstacle CENTER | OBSTACLE_DETECTED: stop, settle 400 ms, wait for a fresh frame, turn toward the side with the lower score |
| Both sides (or all) blocked | STOPPED; retries after 2 s if the path clears |
| Detection uncertain or older than 300 ms | STOPPED |
| Camera failure, link loss | ERROR (STOP), operator must re-arm |
| 8 turns without a forward move | STOPPED, latched until disarmed |

A 2 s cooldown prevents reversing turn direction when both sides are free,
which stops left-right oscillation. Ties keep the previous turn direction.

**UART** (`serial_communication.cpp`): sends the command on change and every
200 ms as a heartbeat, at most once per 50 ms, only when the TX FIFO has room
(never blocks the camera loop). Reads at most 64 bytes per loop. The link is UP
only while ACKs arrive within 800 ms; otherwise it sends STOP.

## Testing plan and results

| Stage | Plan | Result |
|---|---|---|
| 1 | Compile; camera init; capture; stable heap | Not done: ESP32 toolchain unavailable in the build environment. Run `pio run` first. |
| 2 | Image processing and region classification; navigation with simulated detections | **Done on host:** 92 checks pass across protocol, image processing, detection (synthetic frames) and navigation (scenarios plus a 200,000-step safety fuzz) |
| 3 | UART without motors: frames and ACKs | Not done. Needs Nano protocol. A USB-serial adapter on GPIO14 at 9600 baud should show `$n,STOP,0*HH` lines every 200 ms once UART is enabled. |
| 4 | Nano integration, STOP and timeout, wheels off the ground | Not done |
| 5 | Slow autonomous runs with controlled obstacles | Not done |

## Known limitations

- Appearance heuristic only: floor-coloured obstacles, shadows, reflections,
  patterned floors, glass and overhanging objects can fool it. No distance
  estimate. Collision-free navigation is **not** guaranteed.
- Floor model assumes the start view is clear, uniform floor.
- HC-SR04 is not used (no wiring found in the repo). Adding it on the Nano as
  an independent close-range stop is strongly recommended.
- Turns are open-loop timed turns; angle varies with battery and floor.
- UART at 9600 baud keeps SoftwareSerial reliable but limits frame rate.
- Values in `rover_config.h` are starting points and need tuning on the rover.
