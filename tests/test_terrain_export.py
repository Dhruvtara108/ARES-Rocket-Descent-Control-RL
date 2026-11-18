"""Lightweight checks for the self-contained Colab notebook export helpers."""

import ast
import json
import unittest
import zipfile
from pathlib import Path

import numpy as np


NOTEBOOK_PATH = Path(__file__).parents[1] / "notebooks" / "ares_terrain_perception.ipynb"


def load_notebook_helpers():
    notebook = json.loads(NOTEBOOK_PATH.read_text(encoding="utf-8"))
    helper_source = "".join(notebook["cells"][3]["source"])
    tree = ast.parse(helper_source)
    helper_names = {
        "build_terrain_metadata", "fit_image_dimensions", "pop_next_upload",
        "predicted_class_ids", "raw_depth_float32", "create_output_archive",
    }
    definitions = [
        node for node in tree.body
        if isinstance(node, ast.FunctionDef) and node.name in helper_names
    ]
    if {node.name for node in definitions} != helper_names:
        raise AssertionError("Expected metadata, sizing, and ZIP helpers were not found in the notebook.")
    namespace = {"Path": Path, "zipfile": zipfile, "np": np}
    module = ast.Module(body=definitions, type_ignores=[])
    exec(compile(module, str(NOTEBOOK_PATH), "exec"), namespace)
    return notebook, namespace


class TerrainNotebookExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.notebook, cls.helpers = load_notebook_helpers()

    def test_resize_dimensions_preserve_aspect_and_cap_long_side(self):
        fit = self.helpers["fit_image_dimensions"]
        self.assertEqual(fit(4000, 2000, 1024), (1024, 512))
        self.assertEqual(fit(3000, 4000, 1024), (768, 1024))
        self.assertEqual(fit(640, 480, 1024), (640, 480))
        with self.assertRaises(ValueError):
            fit(0, 480, 1024)

    def test_upload_queue_consumes_multiple_files_in_order(self):
        pop_next = self.helpers["pop_next_upload"]
        queue = {"first.jpg": b"one", "second.webp": b"two"}
        self.assertEqual(pop_next(queue), ("first.jpg", b"one"))
        self.assertEqual(pop_next(queue), ("second.webp", b"two"))
        self.assertEqual(queue, {})

    def test_class_ids_are_argmax_predictions_and_depth_is_saved_as_float32(self):
        class FakePrediction:
            def __init__(self, values):
                self.values = values

            def __getitem__(self, index):
                return FakePrediction(self.values[index])

            def cpu(self):
                return self

            def numpy(self):
                return self.values

            def astype(self, dtype):
                return self.values.astype(dtype)

        class FakeLogits:
            def __init__(self, values):
                self.values = values

            def argmax(self, dim):
                return FakePrediction(np.argmax(self.values, axis=dim))

        logits = np.array([[[[0, 4]], [[3, 1]], [[2, 0]]]], dtype=np.float32)
        class_ids = self.helpers["predicted_class_ids"](FakeLogits(logits))
        np.testing.assert_array_equal(class_ids, np.array([[1, 0]], dtype=np.uint8))

        class FakeDepthValues:
            def __init__(self, values):
                self.values = values

            def float(self):
                return self

            def cpu(self):
                return self

            def numpy(self):
                return self.values

        raw_depth = self.helpers["raw_depth_float32"](
            FakeDepthValues(np.array([[1.25, 2.5]], dtype=np.float64))
        )
        self.assertEqual(raw_depth.dtype, np.float32)
        import io

        npy_contents = io.BytesIO()
        np.save(npy_contents, raw_depth)
        npy_contents.seek(0)
        restored = np.load(npy_contents)
        self.assertEqual(restored.dtype, np.float32)
        np.testing.assert_array_equal(restored, raw_depth)

    def test_metadata_is_valid_and_records_class_and_depth_assumptions(self):
        make_metadata = self.helpers["build_terrain_metadata"]
        metadata = make_metadata(
            filename="sample.webp",
            source_dimensions=(2000, 1000),
            image_dimensions=(1024, 512),
            segmentation_model_id="segformer-test",
            depth_model_id="depth-test",
            detected_classes=[{"class_id": 3, "label": "road", "pixel_percentage": 12.5}],
            segmentation_palette={"3": "#808080"},
            depth_min=0.25,
            depth_max=9.5,
        )
        encoded = json.dumps(metadata, allow_nan=False)
        decoded = json.loads(encoded)
        self.assertEqual(decoded["schema_version"], "ares.terrain.v1")
        self.assertEqual(decoded["segmentation"]["classes"][0]["class_id"], 3)
        self.assertEqual(decoded["segmentation"]["classes"][0]["label"], "road")
        self.assertEqual(decoded["depth"]["raw_dtype"], "float32")
        self.assertEqual(decoded["depth"]["calibration_status"], "uncalibrated_relative_depth")
        self.assertFalse(decoded["coordinate_and_measurement_assumptions"]["physical_scale_available"])

    def test_archive_uses_required_root_and_contains_batch_summary_and_image_files(self):
        create_archive = self.helpers["create_output_archive"]
        expected_files = {
            "original_image.png", "segmentation_mask.png", "segmentation_class_ids.png",
            "segmentation_overlay.png", "depth_map.png", "depth_raw.npy", "terrain_metadata.json",
        }
        with self.subTest("archive paths"):
            import tempfile

            with tempfile.TemporaryDirectory() as temporary_directory:
                root = Path(temporary_directory) / "outputs"
                image_directory = root / "image_001"
                image_directory.mkdir(parents=True)
                for filename in expected_files:
                    (image_directory / filename).write_bytes(b"test artifact")
                (root / "summary.json").write_text('{"processed_images": []}', encoding="utf-8")
                archive_path = Path(temporary_directory) / "export.zip"
                create_archive(root, archive_path)
                with zipfile.ZipFile(archive_path) as archive:
                    names = set(archive.namelist())
                self.assertIn("ares_terrain_outputs/summary.json", names)
                self.assertEqual(
                    {Path(name).name for name in names if "/image_001/" in name},
                    expected_files,
                )
                self.assertTrue(all(name.startswith("ares_terrain_outputs/") for name in names))

    def test_notebook_uses_sequential_multi_upload_and_model_predictions(self):
        source = "\n".join(
            "".join(cell.get("source", []))
            for cell in self.notebook["cells"]
            if cell["cell_type"] == "code"
        )
        self.assertIn("files.upload()", source)
        self.assertIn("pop_next_upload(uploaded_files)", source)
        self.assertIn("range(1, image_count + 1)", source)
        self.assertIn("MAX_IMAGE_DIMENSION = 1024", source)
        self.assertIn("predicted_class_ids(seg_logits)", source)
        self.assertIn("segmentation_model.config.id2label", source)
        self.assertIn("astype(np.float32, copy=False)", source)
        self.assertIn("np.save(image_directory / 'depth_raw.npy'", source)


if __name__ == "__main__":
    unittest.main()
