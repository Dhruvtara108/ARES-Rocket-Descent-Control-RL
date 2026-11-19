// Appearance-based ground obstacle detection for a monocular grayscale camera.
//
// Method (heuristic, not object recognition):
//  1. Learn a "floor" model (mean, std) from a reference patch at the bottom
//     centre of the image while the rover is known to face clear floor.
//  2. In the lower part of the image, mark a pixel as obstacle-like if its
//     brightness differs strongly from the floor model; strong Sobel edges count
//     half. Rows nearer the rover are weighted more.
//  3. Split into LEFT / CENTER / RIGHT thirds; each region's raw score is the
//     weighted fraction of obstacle-like pixels (0..1).
//  4. Smooth scores over time (EMA) and switch blocked/clear with hysteresis and
//     multi-frame confirmation.
//
// Known failure modes: floor-coloured obstacles, shadows and reflections, floor
// texture or seams that look like edges, lighting changes, overhanging objects
// above the analysed rows. It provides a hint for cautious navigation only.
#pragma once

#include <stdint.h>

#include "image_processing.h"

enum class Region : uint8_t { LEFT = 0, CENTER = 1, RIGHT = 2 };
constexpr int REGION_COUNT = 3;
const char* regionName(Region r);

enum class DetectStatus : uint8_t { OK = 0, UNCERTAIN };

struct RegionResult {
  float raw = 0.0f;        // this frame's weighted obstacle fraction, 0..1
  float filtered = 0.0f;   // EMA-smoothed score
  bool blocked = false;    // hysteresis + confirmation decision
  float confidence = 0.0f; // heuristic 0..1: distance of score from the switching threshold
};

struct DetectionResult {
  DetectStatus status = DetectStatus::UNCERTAIN;
  const char* reason = "no frame";  // why UNCERTAIN (static string)
  RegionResult region[REGION_COUNT];
  bool obstacle = false;   // any region blocked
  int primary = -1;        // most significant blocked region, -1 if none
  bool pathClear = false;  // status OK and CENTER clear
  float confidence = 0.0f; // minimum region confidence
  float imageMean = 0.0f, imageStd = 0.0f;
  float floorMean = 0.0f, floorStd = 0.0f;
  uint32_t frameId = 0;
  uint32_t timestampMs = 0;
};

class ObstacleDetector {
 public:
  void reset();
  DetectionResult process(const ProcImage& img, uint32_t nowMs);
  bool floorModelReady() const { return floorReady_; }

 private:
  void computeRawScores(const ProcImage& img, float raw[REGION_COUNT]) const;

  bool floorReady_ = false;
  uint8_t warmup_ = 0;
  float floorMean_ = 0.0f;
  float floorStd_ = 0.0f;
  float filtered_[REGION_COUNT] = {0, 0, 0};
  bool blocked_[REGION_COUNT] = {false, false, false};
  uint8_t pending_[REGION_COUNT] = {0, 0, 0};
  bool primed_ = false;
  uint32_t frameId_ = 0;
};
