# Dataset inspection

The ten supplied JPEGs were decoded and reviewed in the contact sheet at `outputs/reports/dataset_contact_sheet.png`. They all show unpaved road or path scenes with vegetation and rocks. The road appearance is similar, but the vegetation, terrain, bends, lighting, and camera direction change substantially. The set does not provide a clear sequence of nearby frames from a single camera traversal; no particular pair can be confidently identified by visual inspection as overlapping views of the same static location.

All 45 unique pairs were evaluated with ORB descriptors, a Hamming BFMatcher, a 0.75 ratio test, and fundamental-matrix RANSAC. The largest observed inlier count was seven. This is insufficient evidence for reliable two-view reconstruction. Repeated dirt and foliage textures can create accidental correspondences and do not establish physical overlap.

**Classification: unsuitable for reconstruction as supplied.** The app therefore does not estimate a pose, triangulate, or emit a PLY for this set. For a useful next capture, take a continuous series of sharp, static-scene photos from nearby camera positions, with roughly 60–80% view overlap, stable exposure, and visible textured landmarks. Record camera calibration at the same image size; metric scale additionally needs a known baseline or reference dimension.

Appearance-based features do not measure terrain elevation, depth, or obstacle distance. The current contact sheet and feature matches are useful diagnostics only.
