"""HTTP server for receiving JPEG images from an ESP32-CAM."""

from __future__ import annotations

import argparse
import json
import socket
import uuid
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


MAX_IMAGE_SIZE = 5 * 1024 * 1024
JPEG_CONTENT_TYPE = "image/jpeg"
JPEG_START = b"\xff\xd8\xff"
JPEG_END = b"\xff\xd9"


class ImageStore:
    """Persist validated images and track the most recent upload."""

    def __init__(self, directory: Path) -> None:
        self.directory = directory
        self.directory.mkdir(parents=True, exist_ok=True)
        self._latest: Path | None = None

    @property
    def latest(self) -> Path | None:
        return self._latest

    def save(self, image: bytes) -> Path:
        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
        for attempt in range(10):
            filename = f"{timestamp}_{uuid.uuid4().hex}_{attempt}.jpg"
            destination = self.directory / filename
            try:
                with destination.open("xb") as image_file:
                    image_file.write(image)
            except FileExistsError:
                continue
            self._latest = destination
            return destination
        raise OSError("Could not create a unique image filename")


class ImageRequestHandler(BaseHTTPRequestHandler):
    """Handle health checks and JPEG image requests."""

    server: "ImageHTTPServer"

    def do_GET(self) -> None:
        if self.path == "/health":
            self._send_json(
                HTTPStatus.OK,
                {
                    "status": "ok",
                    "service": "ARES image input",
                    "latest_image_available": self.server.store.latest is not None,
                },
            )
            return

        if self.path == "/api/images/latest":
            latest = self.server.store.latest
            if latest is None:
                self._send_error(HTTPStatus.NOT_FOUND, "No image has been received yet")
                return
            try:
                image = latest.read_bytes()
            except OSError:
                self._send_error(HTTPStatus.INTERNAL_SERVER_ERROR, "The latest image could not be read")
                return
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", JPEG_CONTENT_TYPE)
            self.send_header("Content-Length", str(len(image)))
            self.end_headers()
            self.wfile.write(image)
            return

        self._send_error(HTTPStatus.NOT_FOUND, "Endpoint not found")

    def do_POST(self) -> None:
        if self.path != "/api/images":
            self._send_error(HTTPStatus.NOT_FOUND, "Endpoint not found")
            return

        if self.headers.get("Content-Type") != JPEG_CONTENT_TYPE:
            self._send_error(HTTPStatus.UNSUPPORTED_MEDIA_TYPE, "Content-Type must be image/jpeg")
            return

        content_length_header = self.headers.get("Content-Length")
        if content_length_header is None:
            self._send_error(HTTPStatus.LENGTH_REQUIRED, "Content-Length header is required")
            return
        try:
            content_length = int(content_length_header)
        except ValueError:
            self._send_error(HTTPStatus.BAD_REQUEST, "Content-Length must be an integer")
            return
        if content_length < 0:
            self._send_error(HTTPStatus.BAD_REQUEST, "Content-Length cannot be negative")
            return
        if content_length > MAX_IMAGE_SIZE:
            self._send_error(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "Image exceeds the 5 MB maximum size")
            return

        image = self.rfile.read(content_length)
        if not image:
            self._send_error(HTTPStatus.BAD_REQUEST, "Image upload cannot be empty")
            return
        if len(image) != content_length:
            self._send_error(HTTPStatus.BAD_REQUEST, "Request body is shorter than Content-Length")
            return
        if not image.startswith(JPEG_START) or not image.endswith(JPEG_END):
            self._send_error(HTTPStatus.BAD_REQUEST, "Request body is not a valid JPEG image")
            return

        try:
            saved_image = self.server.store.save(image)
        except OSError:
            self._send_error(HTTPStatus.INTERNAL_SERVER_ERROR, "Image could not be stored")
            return

        self._send_json(
            HTTPStatus.CREATED,
            {"message": "Image received", "filename": saved_image.name, "size": len(image)},
        )

    def log_message(self, format: str, *args: Any) -> None:
        return

    def _send_error(self, status: HTTPStatus, message: str) -> None:
        self._send_json(status, {"error": message})

    def _send_json(self, status: HTTPStatus, payload: dict[str, Any]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


class ImageHTTPServer(ThreadingHTTPServer):
    """HTTP server carrying the image store used by request handlers."""

    def __init__(self, address: tuple[str, int], store: ImageStore) -> None:
        self.store = store
        super().__init__(address, ImageRequestHandler)


def create_server(host: str, port: int, image_directory: Path) -> ImageHTTPServer:
    """Create an image server with storage rooted at ``image_directory``."""
    return ImageHTTPServer((host, port), ImageStore(image_directory))


def get_local_ip() -> str:
    """Return the local Wi-Fi/LAN address used for outbound connections."""
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("8.8.8.8", 80))
        return probe.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        probe.close()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Receive JPEG images from an ESP32-CAM")
    parser.add_argument("--host", default="0.0.0.0", help="Interface to bind (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8000, help="Port to bind (default: 8000)")
    parser.add_argument(
        "--image-directory",
        type=Path,
        default=Path("data/images"),
        help="Directory for received images (default: data/images)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    server = create_server(args.host, args.port, args.image_directory)
    address = get_local_ip() if args.host == "0.0.0.0" else args.host
    print(f"ARES image input server listening on http://{address}:{server.server_port}")
    print(f"Images are stored in {args.image_directory.resolve()}")
    print("Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
