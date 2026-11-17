# ARES Image Input

ARES Image Input is the first ARES milestone: a small Python HTTP server that
receives JPEG frames from an ESP32-CAM over a local Wi-Fi network. AI
processing and image segmentation are intentionally out of scope for this
milestone.

## Setup

The server uses only the Python standard library. Python 3.10 or newer is
recommended.

```bash
cd ARES-Image-Input
python3 --version
```

No package installation is required.

## Start the server

From the project directory:

```bash
python3 server.py
```

By default, the server listens on every network interface at port `8000` and
saves images under `data/images/`. The startup output includes the address
that an ESP32-CAM on the same network should use.

Host and port can be changed:

```bash
python3 server.py --host 0.0.0.0 --port 8000
python3 server.py --host 127.0.0.1 --port 9000 --image-directory /tmp/ares-images
```

## API

### `GET /health`

Returns JSON describing server availability:

```json
{
  "status": "ok",
  "service": "ARES image input",
  "latest_image_available": false
}
```

### `POST /api/images`

Sends a JPEG as the raw request body. The request must use
`Content-Type: image/jpeg`. Uploads must be non-empty, have JPEG start and end
signatures, and be no larger than 5 MB.

Successful uploads return `201 Created` and JSON containing the generated
filename and byte count. Images receive unique UTC timestamp-based filenames
and are never overwritten.

### `GET /api/images/latest`

Returns the most recently accepted JPEG with `Content-Type: image/jpeg`.
Returns `404` until the first successful upload.

Validation errors return JSON with an `error` message. Common status codes are
`400` for invalid data, `413` for an upload over 5 MB, `415` for a non-JPEG
content type, and `404` for an unknown endpoint or unavailable latest image.

## Test with curl

Start the server in one terminal, then use another terminal:

```bash
curl http://127.0.0.1:8000/health
curl -X POST http://127.0.0.1:8000/api/images \
  -H "Content-Type: image/jpeg" \
  --data-binary @frame.jpg
curl http://127.0.0.1:8000/api/images/latest --output latest.jpg
```

`frame.jpg` must be a real JPEG file. The server stores received images in
`data/images/`, which is excluded from Git.

## ESP32-CAM request

After connecting the ESP32-CAM to the same Wi-Fi network as the laptop, capture
a JPEG frame and send its bytes to the laptop's local IP:

```text
POST http://<laptop-ip>:8000/api/images
Content-Type: image/jpeg
<JPEG bytes>
```

An ESP32 implementation can use its HTTP client library to set the URL,
`Content-Type` header, and request body to the camera frame buffer. The laptop
firewall must allow inbound TCP traffic on port `8000`.

## Find the laptop IP on Ubuntu

Use either command in a terminal:

```bash
hostname -I
ip addr
```

Choose the IPv4 address on the active Wi-Fi interface (often an address such
as `192.168.1.25`), then use it in the ESP32-CAM URL:

```text
http://192.168.1.25:8000/api/images
```

## Run tests

Tests use temporary directories and do not write to the real image directory:

```bash
python3 -m unittest discover -s tests -v
```
