#include "navigation.h"

#include <math.h>

#include "rover_config.h"

namespace {
const char* const kEstopReason = "emergency stop";
constexpr int L = static_cast<int>(Region::LEFT);
constexpr int C = static_cast<int>(Region::CENTER);
constexpr int R = static_cast<int>(Region::RIGHT);
}  // namespace

const char* navStateName(NavState s) {
  switch (s) {
    case NavState::INITIALIZING: return "INITIALIZING";
    case NavState::IDLE: return "IDLE";
    case NavState::EXPLORING: return "EXPLORING";
    case NavState::OBSTACLE_DETECTED: return "OBSTACLE_DETECTED";
    case NavState::TURNING_LEFT: return "TURNING_LEFT";
    case NavState::TURNING_RIGHT: return "TURNING_RIGHT";
    case NavState::STOPPED: return "STOPPED";
    case NavState::ERROR: return "ERROR";
  }
  return "?";
}

const char* motionName(MotionCmd c) {
  switch (c) {
    case MotionCmd::STOP: return "STOP";
    case MotionCmd::FORWARD: return "FORWARD";
    case MotionCmd::BACKWARD: return "BACKWARD";
    case MotionCmd::LEFT: return "LEFT";
    case MotionCmd::RIGHT: return "RIGHT";
  }
  return "?";
}

void Navigator::reset() { *this = Navigator(); }

bool Navigator::consumeStateChange() {
  bool c = changed_;
  changed_ = false;
  return c;
}

void Navigator::enter(NavState s, uint32_t now, const char* why) {
  state_ = s;
  reason_ = why;
  enteredMs_ = now;
  changed_ = true;
  movingForward_ = false;
  latched_ = false;
}

bool Navigator::fresh(const NavInputs& in) const {
  return in.haveDetection && in.nowMs - in.detection.timestampMs <= cfg::FRAME_MAX_AGE_MS;
}

NavOutput Navigator::update(const NavInputs& in) {
  const uint32_t n = in.nowMs;

  if (state_ == NavState::INITIALIZING) {
    if (in.systemReady) enter(NavState::IDLE, n, "initialised");
    return stop();
  }

  // ---- Global safety checks: any failure means STOP, whatever the state. ----
  if (in.estop) {
    if (!(state_ == NavState::STOPPED && reason_ == kEstopReason)) enter(NavState::STOPPED, n, kEstopReason);
    rearmRequired_ = true;
    return stop();
  }
  if (!in.cameraOk) {
    if (state_ != NavState::ERROR) enter(NavState::ERROR, n, "camera failure");
    rearmRequired_ = true;
    return stop();
  }
  if (!in.linkUp && state_ != NavState::IDLE && state_ != NavState::ERROR) {
    enter(NavState::ERROR, n, "Nano link lost");
    rearmRequired_ = true;
    return stop();
  }
  if (!in.armed) {
    rearmRequired_ = false;  // operator has disarmed: a later arm is a fresh decision
    if (state_ != NavState::IDLE && state_ != NavState::ERROR) enter(NavState::IDLE, n, "disarmed");
  }

  switch (state_) {
    case NavState::ERROR:
      if (in.cameraOk && in.linkUp) enter(NavState::IDLE, n, "recovered; re-arm required");
      return stop();

    case NavState::IDLE:
      if (in.armed && !rearmRequired_ && in.linkUp) {
        consecutiveTurns_ = 0;
        enter(NavState::EXPLORING, n, "armed");
      }
      return stop();

    case NavState::EXPLORING:
      return exploring(in);

    case NavState::OBSTACLE_DETECTED:
      // Stop, let the rover settle, and wait for a frame captured after stopping.
      if (n - enteredMs_ < cfg::SETTLE_MS || !fresh(in) || in.detection.timestampMs < enteredMs_ + cfg::SETTLE_MS) {
        return stop();
      }
      return chooseAvoidance(in);

    case NavState::TURNING_LEFT:
    case NavState::TURNING_RIGHT: {
      const MotionCmd dir = state_ == NavState::TURNING_LEFT ? MotionCmd::LEFT : MotionCmd::RIGHT;
      if (!fresh(in)) {
        enter(NavState::STOPPED, n, "stale camera data during turn");
        return stop();
      }
      if (n - enteredMs_ >= cfg::TURN_DURATION_MS) {
        lastTurn_ = dir;
        lastTurnEndMs_ = n;
        ++consecutiveTurns_;
        enter(NavState::EXPLORING, n, "re-checking after turn");
        return stop();
      }
      return NavOutput{dir, cfg::SPEED_TURN};
    }

    case NavState::STOPPED:
      if (reason_ == kEstopReason) {
        enter(NavState::IDLE, n, "emergency stop cleared; re-arm required");
        return stop();
      }
      if (latched_) return stop();
      if (n - enteredMs_ >= cfg::STOP_RETRY_MS && fresh(in) && in.detection.status == DetectStatus::OK &&
          in.detection.pathClear) {
        enter(NavState::EXPLORING, n, "path clear again");
      }
      return stop();

    case NavState::INITIALIZING:
      break;
  }
  enter(NavState::ERROR, n, "invalid navigation state");
  return stop();
}

NavOutput Navigator::exploring(const NavInputs& in) {
  const uint32_t n = in.nowMs;
  if (!fresh(in)) {
    enter(NavState::STOPPED, n, "stale camera data");
    return stop();
  }
  const DetectionResult& d = in.detection;
  if (d.status != DetectStatus::OK) {
    enter(NavState::STOPPED, n, "detection uncertain");
    return stop();
  }
  if (d.region[C].blocked) {
    enter(NavState::OBSTACLE_DETECTED, n, "obstacle CENTER");
    return stop();
  }
  const bool lb = d.region[L].blocked, rb = d.region[R].blocked;
  if (lb && rb) {
    enter(NavState::STOPPED, n, "both sides blocked");
    return stop();
  }
  if (lb) {
    startTurn(MotionCmd::RIGHT, n, "obstacle LEFT, steering right");
    return stop();
  }
  if (rb) {
    startTurn(MotionCmd::LEFT, n, "obstacle RIGHT, steering left");
    return stop();
  }

  // Path appears clear.
  if (n - enteredMs_ < cfg::SETTLE_MS) return stop();  // look before moving
  if (static_cast<int32_t>(pauseUntilMs_ - n) > 0) return stop();
  if (!movingForward_) {
    movingForward_ = true;
    forwardStartMs_ = n;
    consecutiveTurns_ = 0;
  }
  if (n - forwardStartMs_ >= cfg::FORWARD_MAX_MS) {
    movingForward_ = false;
    pauseUntilMs_ = n + cfg::FORWARD_PAUSE_MS;
    return stop();
  }
  return NavOutput{MotionCmd::FORWARD, cfg::SPEED_FORWARD};
}

NavOutput Navigator::chooseAvoidance(const NavInputs& in) {
  const uint32_t n = in.nowMs;
  const DetectionResult& d = in.detection;
  if (d.status != DetectStatus::OK) {
    enter(NavState::STOPPED, n, "detection uncertain");
    return stop();
  }
  if (!d.region[C].blocked) {
    enter(NavState::EXPLORING, n, "center cleared");
    return stop();
  }
  const bool lf = !d.region[L].blocked, rf = !d.region[R].blocked;
  if (!lf && !rf) {
    enter(NavState::STOPPED, n, "all regions blocked");
    return stop();
  }
  MotionCmd dir;
  if (lf && rf) {
    const float diff = d.region[L].filtered - d.region[R].filtered;
    if (fabsf(diff) <= cfg::TURN_TIE_MARGIN) {
      dir = lastTurn_ != MotionCmd::STOP ? lastTurn_ : MotionCmd::RIGHT;
    } else {
      dir = diff > 0 ? MotionCmd::RIGHT : MotionCmd::LEFT;
    }
    // Anti-oscillation: do not reverse direction inside the cooldown window.
    if (lastTurn_ != MotionCmd::STOP && dir != lastTurn_ && n - lastTurnEndMs_ < cfg::TURN_COOLDOWN_MS) {
      dir = lastTurn_;
    }
  } else {
    dir = lf ? MotionCmd::LEFT : MotionCmd::RIGHT;
  }
  startTurn(dir, n, dir == MotionCmd::LEFT ? "avoiding: LEFT is freer" : "avoiding: RIGHT is freer");
  return stop();  // motion starts on the next tick, from the TURNING state
}

void Navigator::startTurn(MotionCmd dir, uint32_t now, const char* why) {
  if (consecutiveTurns_ >= cfg::MAX_CONSECUTIVE_TURNS) {
    enter(NavState::STOPPED, now, "no clear heading after max turns; disarm to reset");
    latched_ = true;
    return;
  }
  enter(dir == MotionCmd::LEFT ? NavState::TURNING_LEFT : NavState::TURNING_RIGHT, now, why);
}
