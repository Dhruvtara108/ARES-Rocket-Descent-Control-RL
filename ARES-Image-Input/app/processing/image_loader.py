from dataclasses import dataclass
from pathlib import Path

import cv2

from app.config import SUPPORTED_EXTENSIONS


@dataclass
class LoadedImage:
    path: Path
    image: object | None
    status: str
    message: str = ""


def discover_images(directory: Path) -> list[Path]:
    if not directory.is_dir():
        return []
    return sorted(p for p in directory.iterdir() if p.is_file() and p.suffix.lower() in SUPPORTED_EXTENSIONS)


def decode_image_bytes(data: bytes):
    if not data:
        raise ValueError("Image data is empty")
    import numpy as np
    image = cv2.imdecode(np.frombuffer(data, dtype=np.uint8), cv2.IMREAD_COLOR)
    if image is None or image.size == 0:
        raise ValueError("Image content is unreadable or unsupported")
    return image


def detect_image_format(data: bytes) -> str | None:
    if data.startswith(b"\xff\xd8\xff"):
        return ".jpg"
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return ".png"
    if data.startswith(b"BM"):
        return ".bmp"
    return None


def load_image(path: Path) -> LoadedImage:
    if path.suffix.lower() not in SUPPORTED_EXTENSIONS:
        return LoadedImage(path, None, "unsupported", "Unsupported file extension")
    try:
        image = cv2.imread(str(path), cv2.IMREAD_COLOR)
    except Exception as exc:
        return LoadedImage(path, None, "corrupt", str(exc))
    if image is None or image.size == 0:
        return LoadedImage(path, None, "corrupt", "Image cannot be decoded")
    h, w = image.shape[:2]
    if w < 1 or h < 1:
        return LoadedImage(path, None, "invalid", "Image dimensions must be positive")
    return LoadedImage(path, image, "valid")
