from pathlib import Path
from uuid import uuid4

from flask import Flask, jsonify, request

from app.config import MAX_IMAGE_BYTES, RECEIVED_DIR, SUPPORTED_EXTENSIONS
from app.processing.image_loader import decode_image_bytes, detect_image_format


def create_app(received_dir: Path = RECEIVED_DIR, max_upload_bytes: int = MAX_IMAGE_BYTES) -> Flask:
    app = Flask(__name__)
    app.config["MAX_CONTENT_LENGTH"] = max_upload_bytes + 1024 * 1024
    received_dir.mkdir(parents=True, exist_ok=True)

    @app.get("/api/health")
    def health():
        return jsonify(status="ok", service="ARES image input")

    @app.get("/api/images")
    def list_images():
        files = sorted(p.name for p in received_dir.iterdir() if p.is_file() and p.suffix.lower() in SUPPORTED_EXTENSIONS)
        return jsonify(images=files, count=len(files))

    @app.post("/api/images/upload")
    def upload():
        if "image" not in request.files:
            return jsonify(error="Multipart form field 'image' is required"), 400
        uploaded = request.files["image"]
        data = uploaded.read(max_upload_bytes + 1)
        if len(data) > max_upload_bytes:
            return jsonify(error="Upload exceeds maximum allowed size"), 413
        suffix = detect_image_format(data)
        requested_suffix = Path(uploaded.filename or "").suffix.lower()
        if suffix not in SUPPORTED_EXTENSIONS:
            return jsonify(error="Unsupported or invalid image format"), 400
        if requested_suffix and requested_suffix in SUPPORTED_EXTENSIONS and requested_suffix != suffix:
            return jsonify(error="Image content does not match its filename extension"), 400
        try:
            decode_image_bytes(data)
        except ValueError as exc:
            return jsonify(error=str(exc)), 400
        destination = received_dir / f"{uuid4().hex}{suffix}"
        destination.write_bytes(data)
        return jsonify(filename=destination.name, size=len(data)), 201

    @app.errorhandler(413)
    def too_large(_error):
        return jsonify(error="Upload exceeds maximum allowed size"), 413

    return app
