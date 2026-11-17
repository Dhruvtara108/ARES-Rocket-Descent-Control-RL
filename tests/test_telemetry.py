"""Unit tests for LunarLander observation telemetry conversions."""

import math
import unittest

from scripts.telemetry import (
    action_name,
    simulation_elapsed_time,
    telemetry_from_observation,
)


class TelemetryConversionTests(unittest.TestCase):
    def test_position_conversion(self) -> None:
        metrics = telemetry_from_observation([1.0, -1.0, 0, 0, 0, 0, 0, 0])
        self.assertEqual(metrics.x_offset, 10.0)
        self.assertAlmostEqual(metrics.y_offset, -20 / 3)

    def test_velocity_conversion(self) -> None:
        metrics = telemetry_from_observation([0, 0, 1.0, -1.0, 0, 0, 0, 0])
        self.assertEqual(metrics.horizontal_velocity, 5.0)
        self.assertEqual(metrics.vertical_velocity, -7.5)

    def test_total_speed(self) -> None:
        metrics = telemetry_from_observation([0, 0, 1.0, 1.0, 0, 0, 0, 0])
        self.assertAlmostEqual(metrics.speed, math.hypot(5.0, 7.5))

    def test_distance_to_pad(self) -> None:
        metrics = telemetry_from_observation([1.0, 0.0, 0, 0, 0, 0, 0, 0])
        self.assertEqual(metrics.distance_from_pad, 10.0)

    def test_angle_conversion(self) -> None:
        metrics = telemetry_from_observation([0, 0, 0, 0, math.pi, 0, 0, 0])
        self.assertEqual(metrics.angle_degrees, 180.0)

    def test_angular_velocity_conversion(self) -> None:
        metrics = telemetry_from_observation([0, 0, 0, 0, 0, 1.0, 0, 0])
        self.assertEqual(metrics.angular_velocity_radians_per_second, 2.5)
        self.assertAlmostEqual(metrics.angular_velocity_degrees_per_second, math.degrees(2.5))

    def test_simulation_time(self) -> None:
        self.assertEqual(simulation_elapsed_time(100), 2.0)

    def test_action_name_mapping(self) -> None:
        self.assertEqual([action_name(action) for action in range(4)], [
            "COAST", "LEFT_ENGINE", "MAIN_ENGINE", "RIGHT_ENGINE"
        ])
        with self.assertRaises(ValueError):
            action_name(4)


if __name__ == "__main__":
    unittest.main()
