#include "camera.h"

#include <Arduino.h>
#include <esp_camera.h>

#include "log.h"
#include "rover_config.h"

bool CameraModule::initHardware() {
  camera_config_t c = {};
  c.pin_pwdn = cfg::CAM_PIN_PWDN;
  c.pin_reset = cfg::CAM_PIN_RESET;
  c.pin_xclk = cfg::CAM_PIN_XCLK;
  c.pin_sccb_sda = cfg::CAM_PIN_SIOD;
  c.pin_sccb_scl = cfg::CAM_PIN_SIOC;
  c.pin_d7 = cfg::CAM_PIN_D7;
  c.pin_d6 = cfg::CAM_PIN_D6;
  c.pin_d5 = cfg::CAM_PIN_D5;
  c.pin_d4 = cfg::CAM_PIN_D4;
  c.pin_d3 = cfg::CAM_PIN_D3;
  c.pin_d2 = cfg::CAM_PIN_D2;
  c.pin_d1 = cfg::CAM_PIN_D1;
  c.pin_d0 = cfg::CAM_PIN_D0;
  c.pin_vsync = cfg::CAM_PIN_VSYNC;
  c.pin_href = cfg::CAM_PIN_HREF;
  c.pin_pclk = cfg::CAM_PIN_PCLK;
  c.xclk_freq_hz = cfg::CAM_XCLK_HZ;
  c.ledc_timer = LEDC_TIMER_0;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.pixel_format = PIXFORMAT_GRAYSCALE;  // no JPEG decode needed for vision
  c.frame_size = FRAMESIZE_QQVGA;        // 160x120
  c.jpeg_quality = 12;                   // unused for grayscale; kept for JPEG experiments
  c.grab_mode = CAMERA_GRAB_LATEST;      // always process the newest frame
  if (psramFound()) {
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.fb_count = 2;
  } else {
    c.fb_location = CAMERA_FB_IN_DRAM;   // 19.2 KB per QQVGA grayscale frame
    c.fb_count = 1;
    c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    LOGF("CAMERA", "Initialization FAILED: 0x%x (%s)", err, esp_err_to_name(err));
    return false;
  }
  sensor_t* s = esp_camera_sensor_get();
  if (!s) {
    LOGF("CAMERA", "Initialization FAILED: no sensor handle");
    esp_camera_deinit();
    return false;
  }
  pid_ = s->id.PID;
  if (pid_ == cfg::CAM_EXPECTED_PID) {
    LOGF("CAMERA", "Sensor identified: OV3660 (PID 0x%04X)", pid_);
  } else {
    LOGF("CAMERA", "WARNING: sensor PID 0x%04X, expected 0x%04X (OV3660). Check board and rover_config.h", pid_,
         cfg::CAM_EXPECTED_PID);
  }
  LOGF("CAMERA", "Initialization successful: %dx%d grayscale, PSRAM %s, fb_count %d", cfg::CAM_WIDTH, cfg::CAM_HEIGHT,
       psramFound() ? "yes" : "no", c.fb_count);
  return true;
}

bool CameraModule::begin() {
  initialised_ = initHardware();
  ok_ = initialised_;
  consecutiveFails_ = 0;
  if (!ok_) nextReinitMs_ = millis() + cfg::CAM_REINIT_BACKOFF_MS;
  return ok_;
}

bool CameraModule::grab(ProcImage& out, uint32_t& captureMs) {
  if (!ok_) return false;
  const uint32_t t0 = micros();
  camera_fb_t* fb = esp_camera_fb_get();
  bool good = false;
  if (fb) {
    captureMs = millis();
    good = fb->format == PIXFORMAT_GRAYSCALE && static_cast<int>(fb->width) == cfg::CAM_WIDTH &&
           static_cast<int>(fb->height) == cfg::CAM_HEIGHT && fb->len >= fb->width * fb->height &&
           imgproc::downsample(fb->buf, fb->width, fb->height, out);
    esp_camera_fb_return(fb);  // always released before returning
  }
  lastGrabUs_ = micros() - t0;
  if (good) {
    ++framesOk_;
    consecutiveFails_ = 0;
    return true;
  }
  ++framesFailed_;
  LOGF("CAMERA", "Frame capture failed (%s), consecutive %u", fb ? "bad format/size" : "no buffer",
       consecutiveFails_ + 1);
  if (++consecutiveFails_ >= cfg::CAM_MAX_CONSECUTIVE_FAILS) {
    ok_ = false;
    nextReinitMs_ = millis() + cfg::CAM_REINIT_BACKOFF_MS;
    LOGF("CAMERA", "Camera marked FAILED after %u consecutive errors", consecutiveFails_);
  }
  return false;
}

void CameraModule::service(uint32_t nowMs) {
  if (ok_ || static_cast<int32_t>(nowMs - nextReinitMs_) < 0) return;
  LOGF("CAMERA", "Attempting re-initialization");
  if (initialised_) {
    esp_camera_deinit();
    initialised_ = false;
  }
  begin();
}
