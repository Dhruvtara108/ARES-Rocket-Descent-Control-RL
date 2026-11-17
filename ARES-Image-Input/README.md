# ARES Image Input

Lightweight local image acquisition, analysis, ORB matching, and calibrated sparse two-view reconstruction. The local dataset in `data/images/` and uploads in `data/received/` pass through the same processing code. This milestone does not infer elevation, depth, or obstacle distance from appearance.

## Setup and processing

From this directory (Python 3.10+):

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python -m app.main
python -m app.main --include-received
```

The app discovers JPG, JPEG, PNG, and BMP files. It validates decoding, resizes large frames, computes grayscale statistics, color histograms, Canny edges and ORB features, and writes a CSV, per-image diagnostics, contact sheet, pair match visualizations, and a feasibility report under `outputs/`.

## Receiver API

Run on the laptop's local network interface:

```powershell
python -c "from app.receiver.api import create_app; create_app().run(host='0.0.0.0', port=8000)"
```

- `GET /api/health`
- `GET /api/images`
- `POST /api/images/upload` with multipart form field `image`

Example with curl:

```bash
curl -F "image=@frame.jpg" http://127.0.0.1:8000/api/images/upload
curl http://127.0.0.1:8000/api/images
```

Images are content-validated, size-limited (10 MiB), given generated names, and stored in `data/received/`. A camera can post the JPEG using the same multipart endpoint over Wi-Fi.

## Reconstruction

Pairwise matching uses ORB, a Hamming BFMatcher, Lowe ratio filtering, and fundamental matrix RANSAC. Pair diagnostics are reported even when no reconstruction is feasible. To estimate relative pose and triangulate, supply a JSON file with a camera matrix for the processed image dimensions:

```json
{"camera_matrix": [[800, 0, 800], [0, 800, 450], [0, 0, 1]]}
```

```powershell
python -m app.main --calibration camera.json
```

The example values above are illustrative only; use actual camera calibration. Reconstruction is attempted only for a pair with sufficient geometric inliers, and its scale remains unknown absent scale information. Images must show overlapping views of the same static scene. The app cannot establish this from matches alone, so confirm the contact sheet and pair images first. It does not combine unrelated images or fabricate geometry.

## Tests and outputs

```powershell
python -m pytest -q
```

Outputs: `outputs/dataset_summary.csv`, `outputs/processed/`, `outputs/matches/`, `outputs/point_cloud/`, and `outputs/reports/reconstruction_report.txt`. Generated content is reproducible and originals in `data/images/` are only read.

## Architecture and future camera integration

`app/processing` owns shared decoding and image analysis; `app/reconstruction` owns pair matching and camera geometry; `app/receiver` accepts uploads. ESP32-CAM integration can send captured frames to the receiver without changing downstream image processing. Metric reconstruction requires calibration and known scale; a handful of unrelated road photographs is not a valid reconstruction dataset.
