// ARES Rover ESP32-CAM firmware: camera -> obstacle detection -> navigation ->
// validated UART commands to the Arduino Nano (which drives the motors).
//
// Safety model:
//  - Boots in STOP. Never moves until an operator types 'a' (arm) on the USB
//    serial console AND the Nano has acknowledged recent frames.
//  - Any camera failure, stale/uncertain detection or link loss => STOP.
//  - 'e' latches an emergency stop (ESTOP frames to the Nano).
//  - The Nano must independently stop after cfg::NANO_TIMEOUT_MS of silence.

#include <Arduino.h>

#include "camera.h"
#include "image_processing.h"
#include "log.h"
#include "navigation.h"
#include "obstacle_detection.h"
#include "rover_config.h"
#include "serial_communication.h"

namespace {
CameraModule camera;
ObstacleDetector detector;
Navigator navigator;
NanoLink nano;
ProcImage frame;  // static: no per-frame allocation

DetectionResult lastDetection;
bool haveDetection = false;
bool armed = false;
bool estop = false;
bool systemReady = false;
uint32_t lastFrameMs = 0;
uint32_t lastVisionLogMs = 0;
uint32_t lastHeapLogMs = 0;
uint32_t lastProcessUs = 0;
MotionCmd lastCmd = MotionCmd::STOP;

void printStatus(uint32_t now) {
  LOGF("STATUS", "state=%s (%s) armed=%d estop=%d camera=%s link=%s frames ok/fail=%lu/%lu tx/ack/err=%lu/%lu/%lu",
       navStateName(navigator.state()), navigator.reason(), armed, estop, camera.ok() ? "OK" : "FAIL",
       nano.linkUp(now) ? "UP" : (nano.started() ? "DOWN" : "DISABLED"), (unsigned long)camera.framesOk(),
       (unsigned long)camera.framesFailed(), (unsigned long)nano.framesSent(), (unsigned long)nano.acks(),
       (unsigned long)nano.errors());
}

void handleConsole(uint32_t now) {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    switch (c) {
      case 'a':
        if (estop) {
          LOGF("SAFETY", "Cannot arm: emergency stop latched (send 'c' to clear)");
        } else {
          armed = true;
          LOGF("SAFETY", "ARMED by operator");
        }
        break;
      case 'd':
        armed = false;
        LOGF("SAFETY", "DISARMED by operator");
        break;
      case 'e':
        estop = true;
        armed = false;
        nano.setEstop(true);
        LOGF("SAFETY", "EMERGENCY STOP latched");
        break;
      case 'c':
        estop = false;
        armed = false;
        nano.setEstop(false);
        LOGF("SAFETY", "Emergency stop cleared; rover disarmed");
        break;
      case 's':
        printStatus(now);
        break;
      case 'h':
      case '?':
        LOGF("CONSOLE", "a=arm d=disarm e=emergency stop c=clear estop s=status");
        break;
      default:
        break;
    }
  }
}

void logVision(const DetectionResult& d) {
  if (d.status != DetectStatus::OK) {
    LOGF("VISION", "UNCERTAIN: %s (mean %.0f)", d.reason, d.imageMean);
  } else if (d.obstacle) {
    LOGF("VISION", "Obstacle detected in %s | L %.2f C %.2f R %.2f | conf %.2f | %lu us",
         regionName(static_cast<Region>(d.primary)), d.region[0].filtered, d.region[1].filtered,
         d.region[2].filtered, d.confidence, (unsigned long)lastProcessUs);
  } else {
    LOGF("VISION", "Path clear | L %.2f C %.2f R %.2f | conf %.2f | %lu us", d.region[0].filtered,
         d.region[1].filtered, d.region[2].filtered, d.confidence, (unsigned long)lastProcessUs);
  }
}
}  // namespace

void setup() {
  Serial.begin(cfg::USB_LOG_BAUD);
  delay(200);
  LOGF("BOOT", "ARES Rover ESP32-CAM firmware. Motors held at STOP.");
  nano.setDesired(MotionCmd::STOP, 0);
  nano.begin();
  camera.begin();
  detector.reset();
  navigator.reset();
  systemReady = true;
  LOGF("BOOT", "Free heap %lu bytes, PSRAM %lu bytes. Type 'h' for console help.",
       (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
}

void loop() {
  const uint32_t now = millis();
  handleConsole(now);
  nano.poll(now);
  camera.service(now);

  if (camera.ok() && now - lastFrameMs >= cfg::FRAME_PERIOD_MS) {
    lastFrameMs = now;
    uint32_t captureMs = now;
    if (camera.grab(frame, captureMs)) {
      const uint32_t t0 = micros();
      lastDetection = detector.process(frame, captureMs);
      lastProcessUs = micros() - t0;
      haveDetection = true;
      if (now - lastVisionLogMs >= cfg::VISION_LOG_PERIOD_MS) {
        lastVisionLogMs = now;
        logVision(lastDetection);
      }
    }
  }
  if (!camera.ok()) haveDetection = false;

  NavInputs in;
  in.nowMs = millis();
  in.systemReady = systemReady;
  in.cameraOk = camera.ok();
  in.linkUp = nano.linkUp(in.nowMs);
  in.armed = armed;
  in.estop = estop;
  in.haveDetection = haveDetection;
  in.detection = lastDetection;
  const NavOutput out = navigator.update(in);

  if (navigator.consumeStateChange()) {
    LOGF("NAVIGATION", "State %s: %s", navStateName(navigator.state()), navigator.reason());
    if (navigator.state() == NavState::ERROR) armed = false;  // errors always require a fresh arm
  }
  if (out.cmd != lastCmd) {
    LOGF("NAVIGATION", "Selecting %s", motionName(out.cmd));
    lastCmd = out.cmd;
  }
  nano.setDesired(out.cmd, out.speed);
  nano.poll(millis());

  if (now - lastHeapLogMs >= cfg::HEAP_LOG_PERIOD_MS) {
    lastHeapLogMs = now;
    LOGF("MEMORY", "heap free %lu min %lu | psram free %lu | grab %lu us", (unsigned long)ESP.getFreeHeap(),
         (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getFreePsram(), (unsigned long)camera.lastGrabUs());
  }
  delay(1);  // yield to the RTOS idle task / watchdog
}
