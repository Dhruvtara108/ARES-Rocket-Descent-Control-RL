import cv2

from app.config import MAX_DIMENSION


def preprocess(image, max_dimension: int = MAX_DIMENSION) -> dict:
    h, w = image.shape[:2]
    scale = min(1.0, max_dimension / max(h, w))
    if scale < 1.0:
        image = cv2.resize(image, (round(w * scale), round(h * scale)), interpolation=cv2.INTER_AREA)
    gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    edges = cv2.Canny(gray, 80, 160)
    histograms = [cv2.calcHist([image], [c], None, [32], [0, 256]).flatten() for c in range(3)]
    return {"image": image, "gray": gray, "edges": edges, "histograms": histograms,
            "mean": float(gray.mean()), "std": float(gray.std())}
