// ARES ESP32-CAM <-> Arduino Nano line protocol.
// Portable C++ (no STL, no dynamic memory) so the same file can be compiled
// for the ESP32, the ATmega328P Nano, and host unit tests.
//
// Frame format (ASCII, one frame per line):
//   $<seq>,<field1>[,<field2>[,<field3>]]*<HH>\n
// <seq>   decimal 0..65535
// <HH>    XOR of every byte between '$' and '*', two uppercase hex digits
//
// ESP32 -> Nano : $17,FORWARD,110*HH   (commands: FORWARD BACKWARD LEFT RIGHT STOP ESTOP PING)
// Nano  -> ESP32: $17,ACK,FORWARD*HH   or   $17,NAK,<reason>*HH
//                 $0,STATUS,<RUN|STOP|ESTOP|TIMEOUT>*HH   (optional, periodic)
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ares_proto {

enum class Cmd : uint8_t { STOP = 0, FORWARD, BACKWARD, LEFT, RIGHT, ESTOP, PING, INVALID };

constexpr size_t MAX_LINE = 48;
constexpr size_t MAX_FIELDS = 3;
constexpr size_t MAX_FIELD_LEN = 12;

const char* cmdName(Cmd c);
Cmd cmdFromName(const char* s);

uint8_t checksum(const char* s, size_t n);

// Writes a NUL-terminated frame ending in '\n'. Speed is clamped to maxSpeed;
// STOP, ESTOP and PING always carry speed 0. Returns length, or 0 on error.
size_t encodeCommand(char* out, size_t cap, uint16_t seq, Cmd c, uint8_t speed, uint8_t maxSpeed);

// Generic reply/command frame writer (used by the Nano for ACK/NAK/STATUS).
size_t encodeFields(char* out, size_t cap, uint16_t seq, const char* const* fields, size_t n);

struct Frame {
  uint16_t seq;
  uint8_t nfields;
  char field[MAX_FIELDS][MAX_FIELD_LEN + 1];
};

enum class ParseError : uint8_t { OK = 0, NO_START, NO_CHECKSUM, BAD_CHECKSUM, BAD_SEQ, BAD_FIELDS, TOO_LONG };

// Parses one line (without the trailing '\n'; a trailing '\r' is ignored).
ParseError parseFrame(const char* line, size_t len, Frame& out);

// Accumulates bytes into lines with a fixed buffer. Overlong lines are dropped
// whole, so a corrupted stream resynchronises on the next '\n'.
class LineAssembler {
 public:
  // Returns true when a complete line is available via line()/length().
  bool feed(char c);
  const char* line() const { return buf_; }
  size_t length() const { return len_; }
  uint32_t overflows() const { return overflows_; }

 private:
  char buf_[MAX_LINE + 1] = {0};
  size_t len_ = 0;
  size_t fill_ = 0;
  bool discarding_ = false;
  uint32_t overflows_ = 0;
};

}  // namespace ares_proto
