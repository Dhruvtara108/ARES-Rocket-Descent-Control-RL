def assess_dataset(pair_results: list[dict]) -> tuple[str, str]:
    if not pair_results:
        return "unsuitable", "No image pairs were available for overlap analysis."
    best = max(pair_results, key=lambda p: p.get("inliers", 0))
    if best.get("inliers", 0) >= 30:
        return "partially suitable", "At least one pair has useful geometric overlap; verify it depicts the same physical scene before reconstruction."
    return "unsuitable", "No pair has enough RANSAC-consistent matches for reliable sparse reconstruction. Capture multiple overlapping views of one static scene."
