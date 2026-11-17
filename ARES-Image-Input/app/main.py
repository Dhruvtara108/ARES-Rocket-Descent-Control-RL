import argparse
import csv
import itertools
from pathlib import Path

import cv2
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from app.config import IMAGE_DIR, OUTPUT_DIR, RECEIVED_DIR
from app.processing.feature_extraction import extract_orb
from app.processing.image_loader import discover_images, load_image
from app.processing.preprocessing import preprocess
from app.reconstruction.feature_matching import match_images
from app.reconstruction.suitability import assess_dataset
from app.reconstruction.camera_geometry import reconstruct_pair, export_ply
from app.visualization.plots import save_contact_sheet, save_match_visual, save_processing_visuals


def process(source_dirs: list[Path], output: Path, calibration: Path | None = None) -> None:
    output.mkdir(parents=True, exist_ok=True)
    processed_dir, matches_dir = output / "processed", output / "matches"
    cloud_dir, reports_dir = output / "point_cloud", output / "reports"
    for directory in (processed_dir, matches_dir, cloud_dir, reports_dir): directory.mkdir(parents=True, exist_ok=True)
    paths = [p for directory in source_dirs for p in discover_images(directory)]
    rows, records, contact = [], [], []
    for path in paths:
        loaded = load_image(path)
        row = {"filename": str(path), "width": "", "height": "", "format": path.suffix.lower().lstrip("."),
               "file_size_bytes": path.stat().st_size, "validation_status": loaded.status, "orb_keypoints": 0}
        if loaded.image is not None:
            raw_h, raw_w = loaded.image.shape[:2]
            prepared = preprocess(loaded.image)
            keypoints, descriptors = extract_orb(prepared["gray"])
            row.update(width=raw_w, height=raw_h, orb_keypoints=len(keypoints))
            save_processing_visuals(path.stem, prepared, keypoints, processed_dir)
            records.append({"path": path, "image": prepared["image"], "gray": prepared["gray"], "keypoints": keypoints,
                            "descriptors": descriptors})
            contact.append((path.name, prepared["image"]))
        rows.append(row)
    with (output / "dataset_summary.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]) if rows else ["filename", "width", "height", "format", "file_size_bytes", "validation_status", "orb_keypoints"])
        writer.writeheader(); writer.writerows(rows)
    save_contact_sheet(contact, reports_dir / "dataset_contact_sheet.png")
    pairs = []
    for i, j in itertools.combinations(range(len(records)), 2):
        a, b = records[i], records[j]
        result = match_images(a["gray"], b["gray"])
        pairs.append({"a": a["path"].name, "b": b["path"].name, "matches": len(result["matches"]), "inliers": len(result["inliers"]), "result": result, "i": i, "j": j})
        if len(result["matches"]) >= 20:
            save_match_visual(f"{a['path'].stem}__{b['path'].stem}", a["image"], b["image"], result,
                              matches_dir / f"{a['path'].stem}__{b['path'].stem}.png")
    suitability, explanation = assess_dataset(pairs)
    calib = None
    if calibration:
        calib = np.asarray(__import__("json").loads(calibration.read_text(encoding="utf-8"))["camera_matrix"], dtype=float)
    reconstruction = None
    best = max(pairs, key=lambda p: p["inliers"], default=None)
    if suitability != "unsuitable" and best and calib is not None:
        a, b = records[best["i"]], records[best["j"]]
        reconstruction = reconstruct_pair(a["gray"], b["gray"], best["result"], calib)
        if reconstruction["success"]:
            export_ply(cloud_dir / "sparse_points.ply", reconstruction["points"])
            pose = {"reference_image": a["path"].name, "second_image": b["path"].name,
                    "rotation": reconstruction["rotation"].tolist(), "translation_up_to_scale": reconstruction["translation"].tolist(),
                    "triangulated_points": len(reconstruction["points"]), "metric_scale": False}
            (cloud_dir / "camera_poses.json").write_text(__import__("json").dumps(pose, indent=2), encoding="utf-8")
            pts = reconstruction["points"]
            fig = plt.figure(); ax = fig.add_subplot(111, projection="3d"); ax.scatter(pts[:, 0], pts[:, 1], pts[:, 2], s=2)
            ax.set_title("Unscaled sparse reconstruction"); fig.savefig(cloud_dir / "sparse_reconstruction.png"); plt.close(fig)
    lines = ["ARES image acquisition and sparse reconstruction report", f"Images discovered: {len(paths)}",
             f"Successfully processed: {sum(r['validation_status'] == 'valid' for r in rows)}",
             f"Image pairs tested: {len(pairs)}", f"Dataset suitability: {suitability}", f"Assessment: {explanation}",
             "Dataset relationship: images require human visual confirmation; pairwise matches alone do not prove a shared physical scene.",
             "Depth limitation: ORB, color, and edge features do not measure terrain elevation, depth, or obstacle distance."]
    lines.extend(f"Pair {p['a']} / {p['b']}: {p['matches']} ratio-test matches, {p['inliers']} RANSAC inliers" for p in pairs)
    if reconstruction is None:
        lines.append("Reconstruction: not attempted; it requires a promising same-scene pair and camera intrinsics calibration.")
    else:
        lines.append(f"Reconstruction: {'success' if reconstruction['success'] else 'failed'}; {reconstruction['reason']}")
    (reports_dir / "reconstruction_report.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Process local and received ARES images")
    parser.add_argument("--images", type=Path, action="append", help="Image source directory (repeatable)")
    parser.add_argument("--output", type=Path, default=OUTPUT_DIR)
    parser.add_argument("--calibration", type=Path, help="JSON file containing camera_matrix (3x3)")
    parser.add_argument("--include-received", action="store_true")
    args = parser.parse_args()
    sources = args.images or [IMAGE_DIR]
    if args.include_received: sources.append(RECEIVED_DIR)
    process(sources, args.output, args.calibration)


if __name__ == "__main__": main()
