// Deterministic navigation state machine. Pure logic: no Arduino calls, time is
// passed in, so it is unit-tested on a PC with simulated detections.
#pragma once

#include <stdint.h>

#include "obstacle_detection.h"

enum class NavState : uint8_t {
  INITIALIZING = 0,
  IDLE,
  EXPLORING,
  OBSTACLE_DETECTED,
  TURNING_LEFT,
  TURNING_RIGHT,
  STOPPED,
  ERROR
};
const char* navStateName(NavState s);

enum class MotionCmd : uint8_t { STOP = 0, FORWARD, BACKWARD, LEFT, RIGHT };
const char* motionName(MotionCmd c);

struct NavInputs {
  uint32_t nowMs = 0;
  bool systemReady = false;  // setup() finished
  bool cameraOk = false;
  bool linkUp = false;       // Nano acknowledged recently
  bool armed = false;        // operator permission to move
  bool estop = false;        // latched emergency stop
  bool haveDetection = false;
  DetectionResult detection;
};

struct NavOutput {
  MotionCmd cmd = MotionCmd::STOP;
  uint8_t speed = 0;
};

class Navigator {
 public:
  void reset();
  NavOutput update(const NavInputs& in);

  NavState state() const { return state_; }
  const char* reason() const { return reason_; }
  // True once per state change (for logging).
  bool consumeStateChange();
  bool rearmRequired() const { return rearmRequired_; }

 private:
  void enter(NavState s, uint32_t now, const char* why);
  bool fresh(const NavInputs& in) const;
  NavOutput stop() const { return NavOutput{}; }
  NavOutput exploring(const NavInputs& in);
  NavOutput chooseAvoidance(const NavInputs& in);
  void startTurn(MotionCmd dir, uint32_t now, const char* why);

  NavState state_ = NavState::INITIALIZING;
  const char* reason_ = "boot";
  bool changed_ = true;
  uint32_t enteredMs_ = 0;
  bool rearmRequired_ = false;
  bool latched_ = false;  // STOPPED that needs disarm/re-arm (no heading found)

  uint32_t forwardStartMs_ = 0;
  bool movingForward_ = false;
  uint32_t pauseUntilMs_ = 0;

  MotionCmd lastTurn_ = MotionCmd::STOP;
  uint32_t lastTurnEndMs_ = 0;
  uint8_t consecutiveTurns_ = 0;
};
