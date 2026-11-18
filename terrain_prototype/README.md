# ARES Terrain AI — Image Preparation

A lightweight, browser-only image upload and preprocessing interface. Images are read, resized, and converted locally in the browser; this page does not send image data to a server.

## Run

From the repository root, run:

```bash
python -m http.server 8000 --directory terrain_prototype
```

Then open [http://localhost:8000](http://localhost:8000).

No packages or build steps are required. The Python command only serves the static files.

## Use

Choose or drop a JPG, JPEG, PNG, or WEBP image up to 15 MB. The page shows the original and creates a JPEG preview constrained to a maximum width or height of 1024 pixels while preserving aspect ratio. PNG transparency is composited on white. Select **Download JPEG** to save the processed image as `terrain_input.jpg`.

## Manual browser checks

1. Run the server and open the local address above.
2. Upload one image of each supported format and confirm the source name, dimensions, size, and format appear.
3. Try dragging an image onto the upload area.
4. Check a landscape and a portrait image larger than 1024 pixels on one side. Confirm the output's longest side is 1024 pixels and proportions are unchanged.
5. Use a transparent PNG and confirm transparent areas appear white in the JPEG preview and download.
6. Download the output and confirm its name is `terrain_input.jpg` and it opens as a JPEG.
7. Try an unsupported file, a zero-byte file, and an image over 15 MB; each should show a clear error.
8. Repeat at a narrow mobile viewport and confirm both previews and controls remain usable.
