// OV3660 camera on the AI-Thinker ESP32-CAM: init, grayscale capture and
// immediate frame-buffer release into a static downsampled image.
#pragma once

#include <stdint.h>

#include "image_processing.h"

class CameraModule {
 public:
  bool begin();
  // Captures one frame into `out`. Returns false on failure. Never holds a
  // frame buffer after returning.
  bool grab(ProcImage& out, uint32_t& captureMs);
  // Call every loop: re-initialises the camera with backoff after failures.
  void service(uint32_t nowMs);

  bool ok() const { return ok_; }
  uint16_t sensorPid() const { return pid_; }
  uint32_t framesOk() const { return framesOk_; }
  uint32_t framesFailed() const { return framesFailed_; }
  uint32_t lastGrabUs() const { return lastGrabUs_; }

 private:
  bool initHardware();

  bool ok_ = false;
  bool initialised_ = false;
  uint16_t pid_ = 0;
  uint8_t consecutiveFails_ = 0;
  uint32_t framesOk_ = 0;
  uint32_t framesFailed_ = 0;
  uint32_t lastGrabUs_ = 0;
  uint32_t nextReinitMs_ = 0;
};
