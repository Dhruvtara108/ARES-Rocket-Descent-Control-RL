#include "image_processing.h"

#include <math.h>
#include <stdlib.h>

namespace imgproc {

bool downsample(const uint8_t* src, int srcW, int srcH, ProcImage& out) {
  if (!src || srcW < ProcImage::W || srcH < ProcImage::H) return false;
  if (srcW % ProcImage::W != 0 || srcH % ProcImage::H != 0) return false;
  const int fx = srcW / ProcImage::W;
  const int fy = srcH / ProcImage::H;
  const int area = fx * fy;
  for (int y = 0; y < ProcImage::H; ++y) {
    for (int x = 0; x < ProcImage::W; ++x) {
      uint32_t sum = 0;
      const uint8_t* row = src + (y * fy) * srcW + x * fx;
      for (int j = 0; j < fy; ++j, row += srcW) {
        for (int i = 0; i < fx; ++i) sum += row[i];
      }
      out.px[y * ProcImage::W + x] = static_cast<uint8_t>(sum / area);
    }
  }
  return true;
}

void regionStats(const ProcImage& img, int x0, int y0, int x1, int y1, float& mean, float& stddev) {
  uint32_t n = 0;
  uint64_t sum = 0, sq = 0;
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      uint32_t v = img.at(x, y);
      sum += v;
      sq += v * v;
      ++n;
    }
  }
  if (n == 0) {
    mean = stddev = 0.0f;
    return;
  }
  mean = static_cast<float>(sum) / n;
  float var = static_cast<float>(sq) / n - mean * mean;
  stddev = var > 0.0f ? sqrtf(var) : 0.0f;
}

int sobelMagnitude(const ProcImage& img, int x, int y) {
  const int a = img.at(x - 1, y - 1), b = img.at(x, y - 1), c = img.at(x + 1, y - 1);
  const int d = img.at(x - 1, y), f = img.at(x + 1, y);
  const int g = img.at(x - 1, y + 1), h = img.at(x, y + 1), i = img.at(x + 1, y + 1);
  const int gx = (c + 2 * f + i) - (a + 2 * d + g);
  const int gy = (g + 2 * h + i) - (a + 2 * b + c);
  return abs(gx) + abs(gy);
}

}  // namespace imgproc
