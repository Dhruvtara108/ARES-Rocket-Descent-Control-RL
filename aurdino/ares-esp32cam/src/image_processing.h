// Small, allocation-free grayscale image utilities for the ESP32-CAM.
#pragma once

#include <stdint.h>

#include "rover_config.h"

struct ProcImage {
  static constexpr int W = cfg::PROC_W;
  static constexpr int H = cfg::PROC_H;
  uint8_t px[W * H];
  uint8_t at(int x, int y) const { return px[y * W + x]; }
};

namespace imgproc {

// Box-filter downsample of an 8-bit grayscale buffer into ProcImage.
// Source dimensions must be integer multiples of ProcImage::W / H.
bool downsample(const uint8_t* src, int srcW, int srcH, ProcImage& out);

// Mean and standard deviation over an inclusive rectangle.
void regionStats(const ProcImage& img, int x0, int y0, int x1, int y1, float& mean, float& stddev);

// |gx| + |gy| Sobel magnitude at an interior pixel (1 <= x < W-1, 1 <= y < H-1).
int sobelMagnitude(const ProcImage& img, int x, int y);

}  // namespace imgproc
