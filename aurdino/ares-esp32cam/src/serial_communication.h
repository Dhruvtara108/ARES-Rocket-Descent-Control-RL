// Non-blocking UART link to the Arduino Nano using ares_protocol.
#pragma once

#include <stdint.h>

#include "ares_protocol.h"
#include "navigation.h"

class NanoLink {
 public:
  // Returns false (and never transmits) unless cfg::UART_WIRING_CONFIRMED.
  bool begin();
  // Reads replies and sends the desired command on change or heartbeat.
  void poll(uint32_t nowMs);
  void setDesired(MotionCmd cmd, uint8_t speed);
  // Latches ESTOP frames until clearEstop().
  void setEstop(bool on) { estop_ = on; }

  bool started() const { return started_; }
  bool linkUp(uint32_t nowMs) const;
  uint32_t framesSent() const { return sent_; }
  uint32_t acks() const { return acks_; }
  uint32_t errors() const { return errors_; }

 private:
  void handleLine(const char* line, size_t len, uint32_t nowMs);
  bool send(ares_proto::Cmd c, uint8_t speed, uint32_t nowMs);

  bool started_ = false;
  bool estop_ = false;
  ares_proto::Cmd desired_ = ares_proto::Cmd::STOP;
  uint8_t desiredSpeed_ = 0;
  ares_proto::Cmd lastSent_ = ares_proto::Cmd::INVALID;
  uint8_t lastSentSpeed_ = 0;
  uint16_t seq_ = 0;
  uint16_t lastSentSeq_ = 0;
  uint32_t lastSendMs_ = 0;
  uint32_t lastAckMs_ = 0;
  bool everAcked_ = false;
  bool wasUp_ = false;
  uint32_t sent_ = 0, acks_ = 0, errors_ = 0;
  ares_proto::LineAssembler rx_;
};
