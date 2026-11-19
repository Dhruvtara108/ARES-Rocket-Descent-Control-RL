#include "ares_protocol.h"

#include <string.h>

namespace ares_proto {

namespace {
const char* const kNames[] = {"STOP", "FORWARD", "BACKWARD", "LEFT", "RIGHT", "ESTOP", "PING"};
constexpr size_t kNameCount = sizeof(kNames) / sizeof(kNames[0]);
const char kHex[] = "0123456789ABCDEF";

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Appends text; returns false if it would not fit (leaves room for NUL).
bool append(char* out, size_t cap, size_t& pos, const char* s) {
  size_t n = strlen(s);
  if (pos + n + 1 > cap) return false;
  memcpy(out + pos, s, n);
  pos += n;
  return true;
}

bool appendUint(char* out, size_t cap, size_t& pos, uint32_t v) {
  char tmp[11];
  size_t i = 0;
  do {
    tmp[i++] = static_cast<char>('0' + (v % 10));
    v /= 10;
  } while (v && i < sizeof(tmp));
  if (pos + i + 1 > cap) return false;
  while (i) out[pos++] = tmp[--i];
  return true;
}

bool finish(char* out, size_t cap, size_t& pos) {
  // pos points after the last payload byte; payload starts at out[1].
  uint8_t cs = checksum(out + 1, pos - 1);
  if (pos + 4 + 1 > cap) return false;  // "*HH\n" + NUL
  out[pos++] = '*';
  out[pos++] = kHex[cs >> 4];
  out[pos++] = kHex[cs & 0x0F];
  out[pos++] = '\n';
  out[pos] = '\0';
  return pos <= MAX_LINE + 1;
}
}  // namespace

const char* cmdName(Cmd c) {
  size_t i = static_cast<size_t>(c);
  return i < kNameCount ? kNames[i] : "INVALID";
}

Cmd cmdFromName(const char* s) {
  for (size_t i = 0; i < kNameCount; ++i) {
    if (strcmp(s, kNames[i]) == 0) return static_cast<Cmd>(i);
  }
  return Cmd::INVALID;
}

uint8_t checksum(const char* s, size_t n) {
  uint8_t cs = 0;
  for (size_t i = 0; i < n; ++i) cs ^= static_cast<uint8_t>(s[i]);
  return cs;
}

size_t encodeFields(char* out, size_t cap, uint16_t seq, const char* const* fields, size_t n) {
  if (!out || cap < 8 || n == 0 || n > MAX_FIELDS) return 0;
  size_t pos = 0;
  out[pos++] = '$';
  if (!appendUint(out, cap, pos, seq)) return 0;
  for (size_t i = 0; i < n; ++i) {
    if (!fields[i] || strlen(fields[i]) == 0 || strlen(fields[i]) > MAX_FIELD_LEN) return 0;
    if (strpbrk(fields[i], "$*,\r\n")) return 0;
    if (!append(out, cap, pos, ",") || !append(out, cap, pos, fields[i])) return 0;
  }
  return finish(out, cap, pos) ? pos : 0;
}

size_t encodeCommand(char* out, size_t cap, uint16_t seq, Cmd c, uint8_t speed, uint8_t maxSpeed) {
  if (c == Cmd::INVALID) return 0;
  if (c == Cmd::STOP || c == Cmd::ESTOP || c == Cmd::PING) speed = 0;
  if (speed > maxSpeed) speed = maxSpeed;
  char sp[4];
  size_t p = 0;
  if (!appendUint(sp, sizeof(sp), p, speed)) return 0;
  sp[p] = '\0';
  const char* f[2] = {cmdName(c), sp};
  return encodeFields(out, cap, seq, f, 2);
}

ParseError parseFrame(const char* line, size_t len, Frame& out) {
  if (len && line[len - 1] == '\r') --len;
  if (len > MAX_LINE) return ParseError::TOO_LONG;
  if (len < 6 || line[0] != '$') return ParseError::NO_START;
  if (len < 4 || line[len - 3] != '*') return ParseError::NO_CHECKSUM;
  int hi = hexVal(line[len - 2]), lo = hexVal(line[len - 1]);
  if (hi < 0 || lo < 0) return ParseError::NO_CHECKSUM;
  size_t payloadLen = len - 4;  // between '$' and '*'
  if (checksum(line + 1, payloadLen) != static_cast<uint8_t>((hi << 4) | lo)) return ParseError::BAD_CHECKSUM;

  // Sequence number.
  size_t i = 1, end = len - 3;
  uint32_t seq = 0;
  size_t digits = 0;
  while (i < end && line[i] >= '0' && line[i] <= '9') {
    seq = seq * 10 + static_cast<uint32_t>(line[i] - '0');
    if (seq > 65535) return ParseError::BAD_SEQ;
    ++i;
    ++digits;
  }
  if (digits == 0 || i >= end || line[i] != ',') return ParseError::BAD_SEQ;
  out.seq = static_cast<uint16_t>(seq);
  out.nfields = 0;

  // Fields.
  while (i < end && line[i] == ',') {
    ++i;
    if (out.nfields >= MAX_FIELDS) return ParseError::BAD_FIELDS;
    size_t start = i;
    while (i < end && line[i] != ',') ++i;
    size_t flen = i - start;
    if (flen == 0 || flen > MAX_FIELD_LEN) return ParseError::BAD_FIELDS;
    memcpy(out.field[out.nfields], line + start, flen);
    out.field[out.nfields][flen] = '\0';
    ++out.nfields;
  }
  if (i != end || out.nfields == 0) return ParseError::BAD_FIELDS;
  return ParseError::OK;
}

bool LineAssembler::feed(char c) {
  if (c == '\n') {
    bool ok = !discarding_ && fill_ > 0;
    buf_[fill_] = '\0';
    len_ = ok ? fill_ : 0;
    fill_ = 0;
    discarding_ = false;
    return ok;
  }
  if (discarding_) return false;
  if (c == '$') fill_ = 0;  // resynchronise on any start marker
  if (fill_ >= MAX_LINE) {
    discarding_ = true;
    fill_ = 0;
    ++overflows_;
    return false;
  }
  buf_[fill_++] = c;
  return false;
}

}  // namespace ares_proto
