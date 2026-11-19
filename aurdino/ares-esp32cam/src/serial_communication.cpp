#include "serial_communication.h"

#include <Arduino.h>
#include <string.h>

#include "log.h"
#include "rover_config.h"

namespace {
HardwareSerial NanoSerial(2);

ares_proto::Cmd toProto(MotionCmd m) {
  switch (m) {
    case MotionCmd::FORWARD: return ares_proto::Cmd::FORWARD;
    case MotionCmd::BACKWARD: return ares_proto::Cmd::BACKWARD;
    case MotionCmd::LEFT: return ares_proto::Cmd::LEFT;
    case MotionCmd::RIGHT: return ares_proto::Cmd::RIGHT;
    case MotionCmd::STOP: break;
  }
  return ares_proto::Cmd::STOP;
}
}  // namespace

bool NanoLink::begin() {
  if (!cfg::UART_WIRING_CONFIRMED) {
    LOGF("UART", "DISABLED: set cfg::UART_WIRING_CONFIRMED after checking wiring. Rover cannot move.");
    return false;
  }
  NanoSerial.begin(cfg::NANO_UART_BAUD, SERIAL_8N1, cfg::NANO_UART_RX_PIN, cfg::NANO_UART_TX_PIN);
  started_ = true;
  LOGF("UART", "Nano link started: %lu baud, RX GPIO%d, TX GPIO%d", (unsigned long)cfg::NANO_UART_BAUD,
       cfg::NANO_UART_RX_PIN, cfg::NANO_UART_TX_PIN);
  return true;
}

bool NanoLink::linkUp(uint32_t nowMs) const {
  return started_ && everAcked_ && nowMs - lastAckMs_ <= cfg::LINK_TIMEOUT_MS;
}

void NanoLink::setDesired(MotionCmd cmd, uint8_t speed) {
  desired_ = toProto(cmd);
  desiredSpeed_ = speed;
}

bool NanoLink::send(ares_proto::Cmd c, uint8_t speed, uint32_t nowMs) {
  char buf[ares_proto::MAX_LINE + 2];
  const uint16_t seq = ++seq_;
  const size_t n = ares_proto::encodeCommand(buf, sizeof(buf), seq, c, speed, cfg::SPEED_MAX);
  if (n == 0) {
    ++errors_;
    LOGF("UART", "Encode error for %s", ares_proto::cmdName(c));
    return false;
  }
  // Non-blocking: skip this cycle if the TX FIFO cannot take the whole frame.
  if (NanoSerial.availableForWrite() < static_cast<int>(n)) return false;
  NanoSerial.write(reinterpret_cast<const uint8_t*>(buf), n);
  const bool changed = c != lastSent_ || speed != lastSentSpeed_;
  lastSent_ = c;
  lastSentSpeed_ = speed;
  lastSentSeq_ = seq;
  lastSendMs_ = nowMs;
  ++sent_;
  if (changed) LOGF("UART", "Command sent: %s speed %u (seq %u)", ares_proto::cmdName(c), speed, seq);
  return true;
}

void NanoLink::handleLine(const char* line, size_t len, uint32_t nowMs) {
  ares_proto::Frame f;
  const ares_proto::ParseError e = ares_proto::parseFrame(line, len, f);
  if (e != ares_proto::ParseError::OK) {
    ++errors_;
    LOGF("UART", "Rejected frame from Nano (error %u)", static_cast<unsigned>(e));
    return;
  }
  if (strcmp(f.field[0], "ACK") == 0 && f.nfields >= 2) {
    // Accept ACKs for any recent sequence number; the Nano answers every frame.
    const uint16_t age = static_cast<uint16_t>(lastSentSeq_ - f.seq);
    if (age > 8) return;
    const bool wasUp = linkUp(nowMs);
    lastAckMs_ = nowMs;
    everAcked_ = true;
    ++acks_;
    if (!wasUp) LOGF("UART", "Nano acknowledgement received (%s): link UP", f.field[1]);
  } else if (strcmp(f.field[0], "NAK") == 0) {
    ++errors_;
    LOGF("UART", "Nano NAK seq %u: %s", f.seq, f.nfields >= 2 ? f.field[1] : "?");
  } else if (strcmp(f.field[0], "STATUS") == 0) {
    static char lastStatus[ares_proto::MAX_FIELD_LEN + 1] = "";
    if (f.nfields >= 2 && strcmp(lastStatus, f.field[1]) != 0) {
      strncpy(lastStatus, f.field[1], sizeof(lastStatus) - 1);
      LOGF("UART", "Nano status: %s", lastStatus);
    }
  }
}

void NanoLink::poll(uint32_t nowMs) {
  if (!started_) return;
  // Bounded read per call so the camera loop is never starved.
  for (int i = 0; i < 64 && NanoSerial.available() > 0; ++i) {
    const char c = static_cast<char>(NanoSerial.read());
    if (rx_.feed(c)) handleLine(rx_.line(), rx_.length(), nowMs);
  }

  const bool up = linkUp(nowMs);
  if (wasUp_ && !up) LOGF("SAFETY", "Communication timeout - STOP");
  wasUp_ = up;

  const ares_proto::Cmd want = estop_ ? ares_proto::Cmd::ESTOP : (up ? desired_ : ares_proto::Cmd::STOP);
  const uint8_t speed = (want == desired_) ? desiredSpeed_ : 0;
  const bool changed = want != lastSent_ || speed != lastSentSpeed_;
  const uint32_t since = nowMs - lastSendMs_;
  if (since < cfg::MIN_SEND_GAP_MS) return;
  if (changed || since >= cfg::HEARTBEAT_MS) send(want, speed, nowMs);
}
