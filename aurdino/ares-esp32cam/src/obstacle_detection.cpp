#include "obstacle_detection.h"

#include <math.h>

namespace {
float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
}  // namespace

const char* regionName(Region r) {
  switch (r) {
    case Region::LEFT: return "LEFT";
    case Region::CENTER: return "CENTER";
    case Region::RIGHT: return "RIGHT";
  }
  return "?";
}

void ObstacleDetector::reset() {
  floorReady_ = false;
  warmup_ = 0;
  floorMean_ = floorStd_ = 0.0f;
  for (int r = 0; r < REGION_COUNT; ++r) {
    filtered_[r] = 0.0f;
    blocked_[r] = false;
    pending_[r] = 0;
  }
  primed_ = false;
  frameId_ = 0;
}

void ObstacleDetector::computeRawScores(const ProcImage& img, float raw[REGION_COUNT]) const {
  const float sigma = floorStd_ > 2.0f ? floorStd_ : 2.0f;
  float thr = cfg::FLOOR_K_SIGMA * sigma;
  if (thr < cfg::FLOOR_MIN_DELTA) thr = cfg::FLOOR_MIN_DELTA;

  float num[REGION_COUNT] = {0, 0, 0};
  float den[REGION_COUNT] = {0, 0, 0};
  const int third = ProcImage::W / 3;
  const float span = static_cast<float>(ProcImage::H - 2 - cfg::ROI_TOP);

  for (int y = cfg::ROI_TOP; y < ProcImage::H - 1; ++y) {
    // Rows nearer the rover (bottom of image) weigh up to 2x the top ROI row.
    const float w = 0.5f + 0.5f * (span > 0 ? (y - cfg::ROI_TOP) / span : 1.0f);
    for (int x = 1; x < ProcImage::W - 1; ++x) {
      const int r = x < third ? 0 : (x < 2 * third ? 1 : 2);
      float score = 0.0f;
      if (fabsf(static_cast<float>(img.at(x, y)) - floorMean_) > thr) {
        score = 1.0f;
      } else if (imgproc::sobelMagnitude(img, x, y) > cfg::EDGE_THRESHOLD) {
        score = cfg::EDGE_WEIGHT;
      }
      num[r] += w * score;
      den[r] += w;
    }
  }
  for (int r = 0; r < REGION_COUNT; ++r) raw[r] = den[r] > 0 ? num[r] / den[r] : 0.0f;
}

DetectionResult ObstacleDetector::process(const ProcImage& img, uint32_t nowMs) {
  DetectionResult res;
  res.frameId = ++frameId_;
  res.timestampMs = nowMs;
  imgproc::regionStats(img, 0, 0, ProcImage::W - 1, ProcImage::H - 1, res.imageMean, res.imageStd);

  // Carry the last temporal state into the result so logs stay meaningful.
  for (int r = 0; r < REGION_COUNT; ++r) {
    res.region[r].filtered = filtered_[r];
    res.region[r].blocked = blocked_[r];
  }

  if (res.imageMean < cfg::DARK_MEAN) {
    res.reason = "image too dark";
    return res;
  }
  if (res.imageMean > cfg::BRIGHT_MEAN) {
    res.reason = "image saturated";
    return res;
  }

  float refMean, refStd;
  imgproc::regionStats(img, cfg::REF_X0, cfg::REF_Y0, cfg::REF_X1, cfg::REF_Y1, refMean, refStd);

  if (!floorReady_) {
    if (refStd <= cfg::REF_MAX_STD) {
      if (warmup_ == 0) {
        floorMean_ = refMean;
        floorStd_ = refStd;
      } else {
        floorMean_ += 0.3f * (refMean - floorMean_);
        floorStd_ += 0.3f * (refStd - floorStd_);
      }
      if (++warmup_ >= cfg::WARMUP_FRAMES) floorReady_ = true;
    }
    res.floorMean = floorMean_;
    res.floorStd = floorStd_;
    res.reason = floorReady_ ? "floor model ready" : "learning floor model";
    return res;  // still UNCERTAIN this frame
  }

  float raw[REGION_COUNT];
  computeRawScores(img, raw);
  const bool refDrifted = fabsf(refMean - floorMean_) > cfg::FLOOR_DRIFT_MAX;
  if (refDrifted) raw[static_cast<int>(Region::CENTER)] = 1.0f;  // something covers the floor patch

  res.status = DetectStatus::OK;
  res.reason = "";
  res.confidence = 1.0f;
  float best = -1.0f;
  for (int r = 0; r < REGION_COUNT; ++r) {
    filtered_[r] = primed_ ? filtered_[r] + cfg::SCORE_EMA_ALPHA * (raw[r] - filtered_[r]) : raw[r];
    const float f = filtered_[r];
    const bool wantSwitch = blocked_[r] ? (f <= cfg::BLOCK_OFF) : (f >= cfg::BLOCK_ON);
    if (wantSwitch) {
      if (++pending_[r] >= cfg::CONFIRM_FRAMES) {
        blocked_[r] = !blocked_[r];
        pending_[r] = 0;
      }
    } else {
      pending_[r] = 0;
    }
    RegionResult& rr = res.region[r];
    rr.raw = raw[r];
    rr.filtered = f;
    rr.blocked = blocked_[r];
    rr.confidence = blocked_[r] ? clamp01((f - cfg::BLOCK_OFF) / (1.0f - cfg::BLOCK_OFF))
                                : clamp01((cfg::BLOCK_ON - f) / cfg::BLOCK_ON);
    if (rr.confidence < res.confidence) res.confidence = rr.confidence;
    if (rr.blocked) {
      res.obstacle = true;
      // CENTER dominates; otherwise the higher score wins.
      const float key = f + (r == static_cast<int>(Region::CENTER) ? 1.0f : 0.0f);
      if (key > best) {
        best = key;
        res.primary = r;
      }
    }
  }
  primed_ = true;
  res.pathClear = !blocked_[static_cast<int>(Region::CENTER)];

  // Adapt the floor model slowly, only when the scene looks clear and consistent.
  if (!res.obstacle && refStd <= cfg::REF_MAX_STD && fabsf(refMean - floorMean_) <= cfg::FLOOR_DRIFT_MAX * 0.5f) {
    floorMean_ += cfg::FLOOR_MODEL_ALPHA * (refMean - floorMean_);
    floorStd_ += cfg::FLOOR_MODEL_ALPHA * (refStd - floorStd_);
  }
  res.floorMean = floorMean_;
  res.floorStd = floorStd_;
  return res;
}
