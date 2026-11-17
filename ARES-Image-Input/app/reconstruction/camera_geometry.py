import cv2
import numpy as np


def reconstruct_pair(gray_a, gray_b, match_result: dict, intrinsics=None) -> dict:
    """Recover relative pose and triangulate only with supplied camera calibration."""
    matches = match_result.get("inliers", [])
    if intrinsics is None:
        return {"success": False, "reason": "Camera intrinsics are required for essential-matrix pose recovery.", "points": np.empty((0, 3))}
    if len(matches) < 8:
        return {"success": False, "reason": "Fewer than eight geometric inlier matches.", "points": np.empty((0, 3))}
    kpa, kpb = match_result["keypoints_a"], match_result["keypoints_b"]
    p1 = np.float64([kpa[m.queryIdx].pt for m in matches])
    p2 = np.float64([kpb[m.trainIdx].pt for m in matches])
    K = np.asarray(intrinsics, dtype=float)
    E, mask = cv2.findEssentialMat(p1, p2, K, method=cv2.RANSAC, prob=0.999, threshold=1.0)
    if E is None:
        return {"success": False, "reason": "Essential matrix estimation failed.", "points": np.empty((0, 3))}
    _, R, t, pose_mask = cv2.recoverPose(E, p1, p2, K, mask=mask)
    keep = pose_mask.ravel().astype(bool)
    P1 = K @ np.hstack((np.eye(3), np.zeros((3, 1))))
    P2 = K @ np.hstack((R, t))
    homogeneous = cv2.triangulatePoints(P1, P2, p1[keep].T, p2[keep].T)
    xyz = (homogeneous[:3] / homogeneous[3]).T
    finite = np.isfinite(xyz).all(axis=1) & (xyz[:, 2] > 0)
    xyz = xyz[finite]
    return {"success": len(xyz) >= 8, "reason": "" if len(xyz) >= 8 else "Too few positive-depth triangulated points.",
            "points": xyz, "rotation": R, "translation": t.reshape(3), "pose_inliers": int(keep.sum())}


def export_ply(path, points):
    points = np.asarray(points, dtype=float)
    points = points.reshape((-1, 3))
    with open(path, "w", encoding="ascii") as f:
        f.write(f"ply\nformat ascii 1.0\nelement vertex {len(points)}\nproperty float x\nproperty float y\nproperty float z\nend_header\n")
        for x, y, z in points:
            f.write(f"{x:.7f} {y:.7f} {z:.7f}\n")
