import json
import threading
import unittest
from http.client import HTTPResponse
from pathlib import Path
from tempfile import TemporaryDirectory
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from server import MAX_IMAGE_SIZE, create_server


JPEG = b"\xff\xd8\xff\xe0test-jpeg-data\xff\xd9"


class ImageServerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.storage = TemporaryDirectory()
        self.server = create_server("127.0.0.1", 0, Path(self.storage.name) / "images")
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base_url = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.storage.cleanup()

    def request(self, method: str, path: str, body: bytes = b"", content_type: str | None = None) -> HTTPResponse:
        headers = {}
        if content_type is not None:
            headers["Content-Type"] = content_type
        request = Request(self.base_url + path, data=body, headers=headers, method=method)
        return urlopen(request)

    def error_response(self, method: str, path: str, body: bytes = b"", content_type: str | None = None) -> tuple[int, dict[str, str]]:
        try:
            self.request(method, path, body, content_type)
        except HTTPError as error:
            return error.code, json.loads(error.read())
        self.fail("Request unexpectedly succeeded")

    def test_health_endpoint(self) -> None:
        response = self.request("GET", "/health")
        self.assertEqual(response.status, 200)
        self.assertEqual(json.loads(response.read())["status"], "ok")

    def test_successful_jpeg_upload(self) -> None:
        response = self.request("POST", "/api/images", JPEG, "image/jpeg")
        payload = json.loads(response.read())
        self.assertEqual(response.status, 201)
        self.assertEqual(payload["size"], len(JPEG))
        self.assertEqual(len(list((Path(self.storage.name) / "images").glob("*.jpg"))), 1)

    def test_invalid_content_type(self) -> None:
        status, payload = self.error_response("POST", "/api/images", JPEG, "text/plain")
        self.assertEqual(status, 415)
        self.assertIn("Content-Type", payload["error"])

    def test_invalid_jpeg_data(self) -> None:
        status, payload = self.error_response("POST", "/api/images", b"not-a-jpeg", "image/jpeg")
        self.assertEqual(status, 400)
        self.assertIn("valid JPEG", payload["error"])

    def test_empty_upload(self) -> None:
        status, payload = self.error_response("POST", "/api/images", b"", "image/jpeg")
        self.assertEqual(status, 400)
        self.assertIn("empty", payload["error"])

    def test_oversized_upload(self) -> None:
        oversized = b"\xff\xd8\xff" + b"x" * MAX_IMAGE_SIZE + b"\xff\xd9"
        status, payload = self.error_response("POST", "/api/images", oversized, "image/jpeg")
        self.assertEqual(status, 413)
        self.assertIn("5 MB", payload["error"])

    def test_latest_image_retrieval(self) -> None:
        self.request("POST", "/api/images", JPEG, "image/jpeg").read()
        response = self.request("GET", "/api/images/latest")
        self.assertEqual(response.status, 200)
        self.assertEqual(response.headers["Content-Type"], "image/jpeg")
        self.assertEqual(response.read(), JPEG)


if __name__ == "__main__":
    unittest.main()
