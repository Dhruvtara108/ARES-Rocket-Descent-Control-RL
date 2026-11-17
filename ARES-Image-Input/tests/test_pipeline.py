import io

import cv2
import numpy as np

from app.processing.image_loader import discover_images, load_image
from app.processing.preprocessing import preprocess
from app.processing.feature_extraction import extract_orb
from app.reconstruction.feature_matching import match_images
from app.reconstruction.suitability import assess_dataset
from app.reconstruction.camera_geometry import export_ply
from app.receiver.api import create_app


def jpeg_bytes():
    ok, encoded = cv2.imencode(".jpg", np.full((48, 64, 3), 120, dtype=np.uint8))
    assert ok
    return encoded.tobytes()


def test_discovery_loading_and_corrupt(tmp_path):
    (tmp_path / "x.jpg").write_bytes(jpeg_bytes())
    (tmp_path / "bad.png").write_bytes(b"broken")
    (tmp_path / "ignored.gif").write_bytes(b"x")
    assert len(discover_images(tmp_path)) == 2
    assert load_image(tmp_path / "x.jpg").status == "valid"
    assert load_image(tmp_path / "bad.png").status == "corrupt"
    assert load_image(tmp_path / "ignored.gif").status == "unsupported"
    assert discover_images(tmp_path / "missing") == []


def test_preprocess_and_orb():
    image = np.zeros((80, 100, 3), dtype=np.uint8)
    result = preprocess(image, 50)
    assert max(result["gray"].shape) == 50
    assert result["edges"].shape == result["gray"].shape
    assert len(result["histograms"]) == 3
    points, _ = extract_orb(result["gray"])
    assert isinstance(points, list)


def test_matching_and_suitability():
    img = np.random.default_rng(2).integers(0, 255, (240, 320), dtype=np.uint8)
    result = match_images(img, img)
    assert len(result["matches"]) > 0
    assert assess_dataset([{"inliers": 0}])[0] == "unsuitable"
    assert assess_dataset([{"inliers": 40}])[0] == "partially suitable"


def test_receiver_endpoints_and_upload(tmp_path):
    client = create_app(tmp_path / "received").test_client()
    assert client.get("/api/health").status_code == 200
    data = jpeg_bytes()
    response = client.post("/api/images/upload", data={"image": (io.BytesIO(data), "../../unsafe.jpg")}, content_type="multipart/form-data")
    assert response.status_code == 201
    assert "unsafe" not in response.json["filename"]
    assert client.get("/api/images").json["count"] == 1
    assert client.post("/api/images/upload", data={"image": (io.BytesIO(b"bad"), "bad.jpg")}).status_code == 400
    assert client.post("/api/images/upload", data={"image": (io.BytesIO(data), "wrong.png")}).status_code == 400


def test_receiver_size_limit(tmp_path):
    client = create_app(tmp_path / "received", max_upload_bytes=10).test_client()
    response = client.post("/api/images/upload", data={"image": (io.BytesIO(b"x" * 100), "big.jpg")})
    assert response.status_code == 413


def test_ply_export(tmp_path):
    path = tmp_path / "points.ply"
    export_ply(path, np.array([[1.0, 2.0, 3.0], [-1.0, 0.0, 4.0]]))
    content = path.read_text()
    assert "element vertex 2" in content
    assert "1.0000000 2.0000000 3.0000000" in content
