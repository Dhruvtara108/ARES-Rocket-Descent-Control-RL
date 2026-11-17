from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMAGE_DIR = ROOT / "data" / "images"
RECEIVED_DIR = ROOT / "data" / "received"
OUTPUT_DIR = ROOT / "outputs"
SUPPORTED_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp"}
MAX_IMAGE_BYTES = 10 * 1024 * 1024
MAX_DIMENSION = 1600
