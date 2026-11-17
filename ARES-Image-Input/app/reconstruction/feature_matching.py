import cv2
import numpy as np

from app.processing.feature_extraction import extract_orb


def match_images(gray_a, gray_b, ratio: float = 0.75) -> dict:
    kpa, da = extract_orb(gray_a)
    kpb, db = extract_orb(gray_b)
    result = {"keypoints_a": kpa, "keypoints_b": kpb, "matches": [], "inliers": [], "fundamental": None}
    if da is None or db is None or len(da) < 2 or len(db) < 2:
        return result
    matcher = cv2.BFMatcher(cv2.NORM_HAMMING)
    pairs = matcher.knnMatch(da, db, k=2)
    good = [m for pair in pairs if len(pair) == 2 for m, n in [pair] if m.distance < ratio * n.distance]
    result["matches"] = good
    if len(good) >= 8:
        a = np.float32([kpa[m.queryIdx].pt for m in good])
        b = np.float32([kpb[m.trainIdx].pt for m in good])
        try:
            fundamental, mask = cv2.findFundamentalMat(a, b, cv2.FM_RANSAC, 1.5, 0.995, 2000)
        except cv2.error:
            fundamental, mask = None, None
        result["fundamental"] = fundamental
        if mask is not None:
            result["inliers"] = [m for m, keep in zip(good, mask.ravel()) if keep]
    return result
