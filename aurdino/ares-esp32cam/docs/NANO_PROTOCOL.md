# ESP32-CAM to Arduino Nano protocol

Status: **specified, not yet implemented on the Nano.** The current
`ares-nano/src/main.cpp` only blinks the LED, so it does not read the UART,
does not drive motors and never sends ACKs. Until the Nano implements this
protocol, the ESP32-CAM link stays DOWN and the rover cannot move. That is
intended.

## Physical layer

| Item | Value |
|---|---|
| Format | 8N1, ASCII lines ending in `\n` (`\r\n` accepted) |
| Baud | 9600 (`cfg::NANO_UART_BAUD`; both sides must match) |
| ESP32-CAM pins | TX GPIO14, RX GPIO13 (proposed, see README wiring) |
| Nano pins | Proposed `SoftwareSerial` on D7 (RX) and D8 (TX); any free pins work. Keep D0/D1 for USB upload and debug. |

## Frame format

```
$<seq>,<field1>[,<field2>[,<field3>]]*<HH>\n
```

- `<seq>`: decimal 0 to 65535, incremented by the ESP32 for every frame.
- `<HH>`: XOR of every byte between `$` and `*`, as two uppercase hex digits.
- Maximum line length 48 characters; fields are 1 to 12 characters and may not
  contain `$ * , \r \n`.
- Frames with a bad checksum or format must be ignored (optionally answered
  with NAK). They must never move the motors.

## ESP32-CAM to Nano

| Frame | Meaning | Nano action |
|---|---|---|
| `$n,FORWARD,<speed>*HH` | Drive forward | Both motors forward at `min(speed, NANO_MAX_SPEED)` |
| `$n,BACKWARD,<speed>*HH` | Drive backward | Both motors backward (not used by the current navigator) |
| `$n,LEFT,<speed>*HH` | Turn left in place | Left motor backward, right motor forward |
| `$n,RIGHT,<speed>*HH` | Turn right in place | Left motor forward, right motor backward |
| `$n,STOP,0*HH` | Stop | Both motors off (brake or coast) |
| `$n,ESTOP,0*HH` | Emergency stop | Motors off and **latched** until the Nano is reset (reset button or power cycle) |
| `$n,PING,0*HH` | Keep-alive | No motion change |

`<speed>` is 0 to 255 PWM. The ESP32 already clamps it to `cfg::SPEED_MAX`
(150); the Nano must apply its own clamp as well.

The ESP32 sends the current command whenever it changes and repeats it every
200 ms (`cfg::HEARTBEAT_MS`) as a heartbeat. It never sends faster than one
frame per 50 ms.

## Nano to ESP32-CAM

| Frame | When |
|---|---|
| `$n,ACK,<CMD>*HH` | After every valid command frame, echoing its sequence number and command name |
| `$n,NAK,<REASON>*HH` | Optional, for a frame that parsed but was rejected (for example `BADCMD`, `ESTOP`) |
| `$0,STATUS,<RUN\|STOP\|ESTOP\|TIMEOUT>*HH` | Optional, at most once per second |

The ESP32 treats the link as UP only while it has received an ACK within the
last 800 ms (`cfg::LINK_TIMEOUT_MS`). Without ACKs it sends only STOP and the
navigator stays in IDLE or ERROR.

## Mandatory Nano behaviour

1. Motors **off** in `setup()` before anything else.
2. **Watchdog:** if no valid frame arrives for 500 ms (`NANO_TIMEOUT_MS`), stop
   the motors, independently of the ESP32.
3. Own speed clamp (`NANO_MAX_SPEED`, suggested 150).
4. ESTOP latch: ignore motion commands after ESTOP until the Nano is reset.
5. Non-blocking loop: no `delay()` longer than a few milliseconds.

## Reference Nano loop (not compiled or tested here)

This sketch shows the required structure. It reuses `ares_protocol.h/.cpp`
from `ares-esp32cam/src` (portable C++, no STL). The motor functions are
deliberately left empty: the repository does not say which driver (L298N or
BTS7960) or which Nano pins are used, so they must be filled in by whoever
knows the wiring.

```cpp
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "ares_protocol.h"

SoftwareSerial link(7, 8);             // RX, TX  (VERIFY)
const uint8_t NANO_MAX_SPEED = 150;
const unsigned long NANO_TIMEOUT_MS = 500;

ares_proto::LineAssembler rx;
unsigned long lastValidMs = 0;
bool estopLatched = false;

void motorsStop()                 { /* TODO: driver-specific */ }
void motorsDrive(int8_t left, int8_t right, uint8_t pwm) { /* TODO: -1/0/+1 per side */ }

void reply(uint16_t seq, const char* a, const char* b) {
  char buf[ares_proto::MAX_LINE + 2];
  const char* f[2] = {a, b};
  size_t n = ares_proto::encodeFields(buf, sizeof(buf), seq, f, 2);
  if (n) link.write(reinterpret_cast<const uint8_t*>(buf), n);
}

void setup() {
  motorsStop();                     // first: motors off
  link.begin(9600);
}

void loop() {
  while (link.available()) {
    if (!rx.feed(static_cast<char>(link.read()))) continue;
    ares_proto::Frame f;
    if (ares_proto::parseFrame(rx.line(), rx.length(), f) != ares_proto::ParseError::OK) continue;
    ares_proto::Cmd c = ares_proto::cmdFromName(f.field[0]);
    if (c == ares_proto::Cmd::INVALID) { reply(f.seq, "NAK", "BADCMD"); continue; }
    lastValidMs = millis();
    uint8_t speed = f.nfields >= 2 ? static_cast<uint8_t>(min(atoi(f.field[1]), (int)NANO_MAX_SPEED)) : 0;
    if (c == ares_proto::Cmd::ESTOP) estopLatched = true;
    if (estopLatched) { motorsStop(); reply(f.seq, "NAK", "ESTOP"); continue; }
    switch (c) {
      case ares_proto::Cmd::FORWARD:  motorsDrive(+1, +1, speed); break;
      case ares_proto::Cmd::BACKWARD: motorsDrive(-1, -1, speed); break;
      case ares_proto::Cmd::LEFT:     motorsDrive(-1, +1, speed); break;
      case ares_proto::Cmd::RIGHT:    motorsDrive(+1, -1, speed); break;
      case ares_proto::Cmd::PING:     break;
      default:                        motorsStop(); break;
    }
    reply(f.seq, "ACK", ares_proto::cmdName(c));
  }
  if (millis() - lastValidMs > NANO_TIMEOUT_MS) motorsStop();   // independent watchdog
}
```

Note: while ESTOP is latched this sketch answers NAK, so the ESP32 sees the
link go DOWN and also stops. That is acceptable for an emergency state.
