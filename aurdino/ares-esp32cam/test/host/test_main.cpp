// Host unit tests (no hardware). Synthetic frames only: these verify the logic,
// not real-world detection accuracy.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ares_protocol.h"
#include "image_processing.h"
#include "navigation.h"
#include "obstacle_detection.h"
#include "rover_config.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (cond) {                                                           \
      ++g_pass;                                                           \
    } else {                                                              \
      ++g_fail;                                                           \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                     \
  } while (0)

// ------------------------------------------------------------------ helpers
static uint32_t g_rng = 12345;
static int noise(int amp) {
  g_rng = g_rng * 1103515245u + 12345u;
  return static_cast<int>((g_rng >> 16) % (2 * amp + 1)) - amp;
}
static void floorImage(ProcImage& img, int base = 120, int amp = 4) {
  for (int i = 0; i < ProcImage::W * ProcImage::H; ++i) img.px[i] = static_cast<uint8_t>(base + noise(amp));
}
static void box(ProcImage& img, int x0, int y0, int x1, int y1, int v) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) img.px[y * ProcImage::W + x] = static_cast<uint8_t>(v + noise(3));
}
static uint32_t t_ms = 1000;
static DetectionResult feed(ObstacleDetector& d, const ProcImage& img, int frames = 1) {
  DetectionResult r;
  for (int i = 0; i < frames; ++i) {
    t_ms += 100;
    r = d.process(img, t_ms);
  }
  return r;
}
// ------------------------------------------------------------------ protocol
static void testProtocol() {
  std::printf("protocol\n");
  using namespace ares_proto;
  char buf[64];
  size_t n = encodeCommand(buf, sizeof(buf), 7, Cmd::FORWARD, 200, 150);
  CHECK(n > 0 && buf[n - 1] == '\n');
  CHECK(std::strncmp(buf, "$7,FORWARD,150*", 15) == 0);  // speed clamped
  Frame f;
  CHECK(parseFrame(buf, n - 1, f) == ParseError::OK);
  CHECK(f.seq == 7 && f.nfields == 2 && std::strcmp(f.field[0], "FORWARD") == 0 && std::strcmp(f.field[1], "150") == 0);

  n = encodeCommand(buf, sizeof(buf), 8, Cmd::STOP, 99, 150);
  CHECK(std::strncmp(buf, "$8,STOP,0*", 10) == 0);  // STOP never carries speed
  CHECK(encodeCommand(buf, sizeof(buf), 1, Cmd::INVALID, 0, 150) == 0);
  CHECK(encodeCommand(buf, 8, 1, Cmd::FORWARD, 10, 150) == 0);  // too small buffer

  // Corruption is rejected.
  n = encodeCommand(buf, sizeof(buf), 9, Cmd::LEFT, 100, 150);
  buf[4] = 'X';
  CHECK(parseFrame(buf, n - 1, f) == ParseError::BAD_CHECKSUM);
  CHECK(parseFrame("9,LEFT,1*00", 11, f) == ParseError::NO_START);
  CHECK(parseFrame("$9,LEFT,100", 11, f) == ParseError::NO_CHECKSUM);
  char longLine[80];
  std::memset(longLine, 'A', sizeof(longLine));
  CHECK(parseFrame(longLine, sizeof(longLine), f) == ParseError::TOO_LONG);

  // Reply frames, CRLF tolerated.
  const char* fields[2] = {"ACK", "FORWARD"};
  n = encodeFields(buf, sizeof(buf), 7, fields, 2);
  buf[n - 1] = '\r';
  CHECK(parseFrame(buf, n, f) == ParseError::OK && std::strcmp(f.field[0], "ACK") == 0);
  const char* bad[1] = {"A,B"};
  CHECK(encodeFields(buf, sizeof(buf), 1, bad, 1) == 0);

  CHECK(cmdFromName("RIGHT") == Cmd::RIGHT && cmdFromName("JUMP") == Cmd::INVALID);

  // Line assembler: garbage prefix, overflow and resync.
  LineAssembler la;
  const char* stream = "xx$1,ACK,STOP*";
  for (const char* p = stream; *p; ++p) CHECK(!la.feed(*p));
  char frame[64];
  n = encodeFields(frame, sizeof(frame), 1, fields, 2);
  LineAssembler lb;
  bool got = false;
  for (int i = 0; i < 100; ++i) lb.feed('Z');  // overflow
  CHECK(!lb.feed('\n'));                       // overlong line dropped
  CHECK(lb.overflows() == 1);
  for (size_t i = 0; i < n; ++i) got = lb.feed(frame[i]);
  CHECK(got && parseFrame(lb.line(), lb.length(), f) == ParseError::OK);
}

// ------------------------------------------------------------------ image processing
static void testImageProcessing() {
  std::printf("image processing\n");
  static uint8_t src[160 * 120];
  for (int y = 0; y < 120; ++y)
    for (int x = 0; x < 160; ++x) src[y * 160 + x] = static_cast<uint8_t>((x % 2) ? 100 : 50);
  ProcImage img;
  CHECK(imgproc::downsample(src, 160, 120, img));
  CHECK(img.at(0, 0) == 75 && img.at(79, 59) == 75);
  CHECK(!imgproc::downsample(src, 150, 120, img));
  CHECK(!imgproc::downsample(nullptr, 160, 120, img));

  floorImage(img, 100, 0);
  float m, s;
  imgproc::regionStats(img, 0, 0, 79, 59, m, s);
  CHECK(m > 99.9f && m < 100.1f && s < 0.01f);
  CHECK(imgproc::sobelMagnitude(img, 40, 30) == 0);
  box(img, 40, 0, 79, 59, 200);
  CHECK(imgproc::sobelMagnitude(img, 40, 30) > cfg::EDGE_THRESHOLD);
}

// ------------------------------------------------------------------ detection
static void testDetection() {
  std::printf("obstacle detection\n");
  ObstacleDetector d;
  d.reset();
  ProcImage img;
  floorImage(img);
  DetectionResult r = feed(d, img);
  CHECK(r.status == DetectStatus::UNCERTAIN && !d.floorModelReady());
  for (int i = 0; i < cfg::WARMUP_FRAMES; ++i) {
    floorImage(img);
    r = feed(d, img);
  }
  CHECK(d.floorModelReady());

  // Clear floor.
  for (int i = 0; i < 10; ++i) {
    floorImage(img);
    r = feed(d, img);
  }
  CHECK(r.status == DetectStatus::OK && r.pathClear && !r.obstacle && r.primary == -1);
  CHECK(r.confidence > 0.5f);

  // Single-frame glitch does not trigger (temporal confirmation).
  floorImage(img);
  box(img, 28, 28, 52, 48, 30);
  r = feed(d, img);
  CHECK(!r.region[1].blocked);
  floorImage(img);
  r = feed(d, img, 3);
  CHECK(!r.obstacle);

  // Persistent centre obstacle (above the reference patch).
  for (int i = 0; i < 6; ++i) {
    floorImage(img);
    box(img, 28, 28, 52, 48, 30);
    r = feed(d, img);
  }
  CHECK(r.status == DetectStatus::OK && r.region[1].blocked && !r.pathClear);
  CHECK(r.primary == static_cast<int>(Region::CENTER));
  CHECK(!r.region[0].blocked && !r.region[2].blocked);

  // Removal clears again (hysteresis).
  for (int i = 0; i < 10; ++i) {
    floorImage(img);
    r = feed(d, img);
  }
  CHECK(r.pathClear && !r.obstacle);

  // Left-only and right-only obstacles.
  for (int i = 0; i < 6; ++i) {
    floorImage(img);
    box(img, 2, 26, 24, 58, 210);
    r = feed(d, img);
  }
  CHECK(r.region[0].blocked && !r.region[1].blocked && !r.region[2].blocked && r.primary == 0 && r.pathClear);
  for (int i = 0; i < 10; ++i) {
    floorImage(img);
    feed(d, img);
  }
  for (int i = 0; i < 6; ++i) {
    floorImage(img);
    box(img, 55, 26, 78, 58, 30);
    r = feed(d, img);
  }
  CHECK(r.region[2].blocked && !r.region[0].blocked && !r.region[1].blocked && r.primary == 2);
  for (int i = 0; i < 10; ++i) {
    floorImage(img);
    feed(d, img);
  }

  // Object right in front covering the floor reference patch.
  for (int i = 0; i < 6; ++i) {
    floorImage(img);
    box(img, 26, 40, 54, 59, 200);
    r = feed(d, img);
  }
  CHECK(r.region[1].blocked);
  for (int i = 0; i < 12; ++i) {
    floorImage(img);
    r = feed(d, img);
  }
  CHECK(r.pathClear);

  // Bad exposure => UNCERTAIN.
  floorImage(img, 8, 2);
  r = feed(d, img);
  CHECK(r.status == DetectStatus::UNCERTAIN && !r.pathClear);
  floorImage(img, 250, 2);
  r = feed(d, img);
  CHECK(r.status == DetectStatus::UNCERTAIN);

  // Warm-up rejects a textured reference patch.
  ObstacleDetector d2;
  d2.reset();
  for (int i = 0; i < 20; ++i) {
    floorImage(img, 120, 60);  // very noisy
    feed(d2, img);
  }
  CHECK(!d2.floorModelReady());
}

// ------------------------------------------------------------------ navigation
struct Sim {
  Navigator nav;
  NavInputs in;
  Sim() {
    nav.reset();
    in.nowMs = 0;
    in.systemReady = true;
    in.cameraOk = true;
    in.linkUp = true;
    in.armed = false;
    in.haveDetection = true;
    setDet(false, false, false);
  }
  void setDet(bool l, bool c, bool r, DetectStatus st = DetectStatus::OK, float lf = -1, float rf = -1) {
    DetectionResult& d = in.detection;
    d = DetectionResult();
    d.status = st;
    d.region[0].blocked = l;
    d.region[1].blocked = c;
    d.region[2].blocked = r;
    d.region[0].filtered = lf >= 0 ? lf : (l ? 0.6f : 0.05f);
    d.region[1].filtered = c ? 0.6f : 0.05f;
    d.region[2].filtered = rf >= 0 ? rf : (r ? 0.6f : 0.05f);
    d.obstacle = l || c || r;
    d.pathClear = st == DetectStatus::OK && !c;
    d.timestampMs = in.nowMs;
  }
  // Advance time; detections stay fresh unless `stale`.
  NavOutput step(uint32_t dt = 50, bool stale = false) {
    in.nowMs += dt;
    if (!stale) in.detection.timestampMs = in.nowMs;
    return nav.update(in);
  }
  NavOutput run(uint32_t ms) {
    NavOutput o;
    for (uint32_t t = 0; t < ms; t += 50) o = step();
    return o;
  }
};

static void testNavigation() {
  std::printf("navigation\n");
  {  // Boot and arming gates.
    Sim s;
    CHECK(s.nav.state() == NavState::INITIALIZING);
    CHECK(s.step().cmd == MotionCmd::STOP && s.nav.state() == NavState::IDLE);
    CHECK(s.run(2000).cmd == MotionCmd::STOP && s.nav.state() == NavState::IDLE);  // not armed
    s.in.linkUp = false;
    s.in.armed = true;
    CHECK(s.run(1000).cmd == MotionCmd::STOP && s.nav.state() == NavState::IDLE);  // no link
    s.in.linkUp = true;
    s.step();
    CHECK(s.nav.state() == NavState::EXPLORING);
    CHECK(s.step().cmd == MotionCmd::STOP);  // settle before first move
    CHECK(s.run(cfg::SETTLE_MS).cmd == MotionCmd::FORWARD);
  }
  {  // Forward runs are bounded and followed by a pause.
    Sim s;
    s.in.armed = true;
    s.step();
    s.step();
    s.run(cfg::SETTLE_MS + 50);
    int stops = 0;
    for (uint32_t t = 0; t < cfg::FORWARD_MAX_MS + cfg::FORWARD_PAUSE_MS + 200; t += 50)
      if (s.step().cmd == MotionCmd::STOP) ++stops;
    CHECK(stops >= static_cast<int>(cfg::FORWARD_PAUSE_MS / 50) - 1);
    CHECK(s.step().cmd == MotionCmd::FORWARD);
  }
  {  // Centre obstacle: stop, settle, turn toward the freer side, re-check.
    Sim s;
    s.in.armed = true;
    s.run(cfg::SETTLE_MS + 200);
    s.setDet(false, true, false, DetectStatus::OK, 0.10f, 0.25f);
    CHECK(s.step().cmd == MotionCmd::STOP && s.nav.state() == NavState::OBSTACLE_DETECTED);
    CHECK(s.run(cfg::SETTLE_MS - 100).cmd == MotionCmd::STOP);
    s.run(200);
    CHECK(s.nav.state() == NavState::TURNING_LEFT);
    CHECK(s.step().cmd == MotionCmd::LEFT);
    s.run(cfg::TURN_DURATION_MS);
    CHECK(s.nav.state() == NavState::EXPLORING || s.nav.state() == NavState::OBSTACLE_DETECTED);
  }
  {  // Side obstacles steer away; both blocked stops.
    Sim s;
    s.in.armed = true;
    s.run(cfg::SETTLE_MS + 200);
    s.setDet(true, false, false);
    s.step();
    CHECK(s.nav.state() == NavState::TURNING_RIGHT && s.step().cmd == MotionCmd::RIGHT);
    s.setDet(false, false, false);
    s.run(cfg::TURN_DURATION_MS + cfg::SETTLE_MS + 100);
    s.setDet(false, false, true);
    s.step();
    CHECK(s.nav.state() == NavState::TURNING_LEFT);
    s.run(cfg::TURN_DURATION_MS + 100);
    s.setDet(true, false, true);  // a timed turn completes, then the re-check stops
    s.run(cfg::TURN_DURATION_MS + 100);
    CHECK(s.step().cmd == MotionCmd::STOP && s.nav.state() == NavState::STOPPED);
    CHECK(s.run(1000).cmd == MotionCmd::STOP);
    s.setDet(false, false, false);
    s.run(cfg::STOP_RETRY_MS + 100);
    CHECK(s.nav.state() == NavState::EXPLORING);  // retries once the path is clear
  }
  {  // All regions blocked after stopping.
    Sim s;
    s.in.armed = true;
    s.run(cfg::SETTLE_MS + 200);
    s.setDet(true, true, true);
    s.run(cfg::SETTLE_MS + 200);
    CHECK(s.nav.state() == NavState::STOPPED && s.step().cmd == MotionCmd::STOP);
  }
  {  // Uncertain and stale detections stop the rover.
    Sim s;
    s.in.armed = true;
    s.run(cfg::SETTLE_MS + 200);
    s.setDet(false, false, false, DetectStatus::UNCERTAIN);
    CHECK(s.step().cmd == MotionCmd::STOP && s.nav.state() == NavState::STOPPED);
    Sim t;
    t.in.armed = true;
    t.run(cfg::SETTLE_MS + 200);
    NavOutput o;
    for (int i = 0; i < 10; ++i) o = t.step(50, true);
    CHECK(o.cmd == MotionCmd::STOP && t.nav.state() == NavState::STOPPED);
  }
  {  // Link loss, camera failure, re-arm requirement.
    Sim s;
    s.in.armed = true;
    s.run(cfg::SETTLE_MS + 200);
    s.in.linkUp = false;
    CHECK(s.step().cmd == MotionCmd::STOP && s.nav.state() == NavState::ERROR);
    s.in.linkUp = true;
    s.step();
    CHECK(s.nav.state() == NavState::IDLE && s.nav.rearmRequired());
    CHECK(s.run(1000).cmd == MotionCmd::STOP && s.nav.state() == NavState::IDLE);  // still armed flag, no restart
    s.in.armed = false;
    s.step();
    s.in.armed = true;
    s.step();
    CHECK(s.nav.state() == NavState::EXPLORING);
    s.in.cameraOk = false;
    CHECK(s.step().cmd == MotionCmd::STOP && s.nav.state() == NavState::ERROR);
  }
  {  // Emergency stop.
    Sim s;
    s.in.armed = true;
    s.run(cfg::SETTLE_MS + 200);
    s.in.estop = true;
    CHECK(s.step().cmd == MotionCmd::STOP && s.nav.state() == NavState::STOPPED);
    CHECK(s.run(2000).cmd == MotionCmd::STOP);
    s.in.estop = false;
    s.in.armed = false;
    s.step();
    CHECK(s.nav.state() == NavState::IDLE);
  }
  {  // Anti-oscillation cooldown, then max-turn latch.
    Sim s;
    s.in.armed = true;
    s.run(cfg::SETTLE_MS + 200);
    s.setDet(false, true, false, DetectStatus::OK, 0.25f, 0.05f);  // right freer
    s.run(cfg::SETTLE_MS + 100);
    CHECK(s.nav.state() == NavState::TURNING_RIGHT);
    s.run(cfg::TURN_DURATION_MS + 50);
    s.setDet(false, true, false, DetectStatus::OK, 0.05f, 0.25f);  // now left freer, inside cooldown
    s.run(cfg::SETTLE_MS + 100);
    CHECK(s.nav.state() == NavState::TURNING_RIGHT);  // keeps committed direction
    bool latched = false;
    for (int i = 0; i < 40 && !latched; ++i) {
      s.run(300);
      latched = s.nav.state() == NavState::STOPPED;
    }
    CHECK(latched);
    CHECK(s.run(cfg::STOP_RETRY_MS * 2).cmd == MotionCmd::STOP && s.nav.state() == NavState::STOPPED);
    s.in.armed = false;
    s.step();
    CHECK(s.nav.state() == NavState::IDLE);
  }
  {  // Property: never commands motion when any safety input is bad.
    Sim s;
    uint32_t rng = 99;
    int violations = 0, moves = 0;
    for (int i = 0; i < 200000; ++i) {
      rng = rng * 1664525u + 1013904223u;
      const uint32_t r = rng >> 8;
      // Faults are rare and recover quickly, so motion paths are exercised too.
      auto flip = [&](bool& v, bool good, uint32_t bits) {
        const uint32_t p = v == good ? 0x7FF : 0x1F;  // leave good state rarely, bad state soon
        if ((bits & p) == 0) v = !v;
      };
      flip(s.in.armed, true, r >> 1);
      flip(s.in.linkUp, true, r >> 5);
      flip(s.in.cameraOk, true, r >> 9);
      flip(s.in.estop, false, r >> 13);
      if (((r >> 2) & 0x7F) == 0) {
        const int m = ((r >> 18) & 3) ? 0 : static_cast<int>((r >> 12) & 0xF);
        s.setDet(m & 1, m & 2, m & 4, (m & 8) && ((r >> 20) & 1) ? DetectStatus::UNCERTAIN : DetectStatus::OK);
      }
      const bool stale = (rng & 0x3FF) == 7;
      const NavOutput o = s.step(20 + (r % 60), stale);
      if (s.nav.state() == NavState::ERROR || s.in.estop) s.in.armed = false;  // as main.cpp does
      const bool unsafe = !s.in.armed || s.in.estop || !s.in.cameraOk || !s.in.linkUp ||
                          s.in.nowMs - s.in.detection.timestampMs > cfg::FRAME_MAX_AGE_MS;
      if (o.cmd != MotionCmd::STOP) {
        ++moves;
        if (unsafe) ++violations;
        if (o.cmd == MotionCmd::FORWARD && (s.in.detection.region[1].blocked || s.in.detection.status != DetectStatus::OK))
          ++violations;
        if (o.speed > cfg::SPEED_MAX) ++violations;
      }
    }
    std::printf("  fuzz: %d motion outputs, %d violations\n", moves, violations);
    CHECK(violations == 0);
    CHECK(moves > 0);
  }
}

int main() {
  testProtocol();
  testImageProcessing();
  testDetection();
  testNavigation();
  std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
