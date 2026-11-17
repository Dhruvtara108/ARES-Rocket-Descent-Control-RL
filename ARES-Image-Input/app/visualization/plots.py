from pathlib import Path

import cv2
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def save_processing_visuals(name: str, image: dict, keypoints, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    rgb = cv2.cvtColor(image["image"], cv2.COLOR_BGR2RGB)
    marked = cv2.drawKeypoints(image["image"], keypoints, None, color=(0, 255, 0), flags=0)
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    axes[0, 0].imshow(rgb); axes[0, 0].set_title("Original")
    axes[0, 1].imshow(image["gray"], cmap="gray"); axes[0, 1].set_title("Grayscale")
    axes[1, 0].imshow(image["edges"], cmap="gray"); axes[1, 0].set_title("Canny edges")
    axes[1, 1].imshow(cv2.cvtColor(marked, cv2.COLOR_BGR2RGB)); axes[1, 1].set_title(f"ORB keypoints ({len(keypoints)})")
    for ax in axes.flat: ax.axis("off")
    fig.tight_layout(); fig.savefig(output / f"{name}_diagnostics.png", dpi=120); plt.close(fig)
    fig, ax = plt.subplots(figsize=(8, 4))
    for hist, color, label in zip(image["histograms"], ("b", "g", "r"), ("Blue", "Green", "Red")):
        ax.plot(hist, color=color, label=label)
    ax.set_title("Color histogram"); ax.legend(); fig.tight_layout()
    fig.savefig(output / f"{name}_histogram.png", dpi=120); plt.close(fig)


def save_contact_sheet(images: list[tuple[str, object]], path: Path) -> None:
    if not images: return
    cols, rows = 2, (len(images) + 1) // 2
    fig, axes = plt.subplots(rows, cols, figsize=(12, 4 * rows), squeeze=False)
    for ax, (name, image) in zip(axes.flat, images):
        ax.imshow(cv2.cvtColor(image, cv2.COLOR_BGR2RGB)); ax.set_title(name); ax.axis("off")
    for ax in list(axes.flat)[len(images):]: ax.axis("off")
    fig.tight_layout(); path.parent.mkdir(parents=True, exist_ok=True); fig.savefig(path, dpi=120); plt.close(fig)


def save_match_visual(name: str, a, b, result: dict, path: Path) -> None:
    shown = cv2.drawMatches(a, result["keypoints_a"], b, result["keypoints_b"], result["inliers"], None,
                            flags=cv2.DrawMatchesFlags_NOT_DRAW_SINGLE_POINTS)
    path.parent.mkdir(parents=True, exist_ok=True); cv2.imwrite(str(path), shown)
