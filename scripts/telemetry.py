"""Conversions and derived flight metrics for Gymnasium LunarLander-v3.

Gymnasium observations are normalized from its Box2D world. With a 600x400
viewport, SCALE=30, and FPS=50, the horizontal and vertical half extents are
10 and 20/3 simulation units. Position fields normalize by these extents;
velocity fields normalize by extent/FPS. Angular velocity is normalized by
FPS/20. These are simulation units, not real-world metres or metres/second.
The vertical observation origin is LunarLander's helipad reference plane.
"""

from dataclasses import dataclass
import math
from typing import Sequence


SIMULATION_FPS = 50
VIEWPORT_WIDTH = 600
VIEWPORT_HEIGHT = 400
WORLD_SCALE = 30.0
HORIZONTAL_HALF_EXTENT = VIEWPORT_WIDTH / WORLD_SCALE / 2.0
VERTICAL_HALF_EXTENT = VIEWPORT_HEIGHT / WORLD_SCALE / 2.0
ANGULAR_VELOCITY_SCALE = 20.0

ACTION_NAMES = {
    0: "COAST",
    1: "LEFT_ENGINE",
    2: "MAIN_ENGINE",
    3: "RIGHT_ENGINE",
}


@dataclass(frozen=True)
class FlightTelemetry:
    x_offset: float
    y_offset: float
    horizontal_velocity: float
    vertical_velocity: float
    speed: float
    distance_from_pad: float
    angle_degrees: float
    angular_velocity_radians_per_second: float
    angular_velocity_degrees_per_second: float
    left_leg_contact: bool
    right_leg_contact: bool


def telemetry_from_observation(observation: Sequence[float]) -> FlightTelemetry:
    """Convert LunarLander's eight normalized observation fields to simulation units."""
    if len(observation) != 8:
        raise ValueError(f"Expected an eight-field LunarLander observation, got {len(observation)}")
    x, y, vx, vy, angle, angular_velocity, left_contact, right_contact = map(float, observation)

    x_offset = x * HORIZONTAL_HALF_EXTENT
    y_offset = y * VERTICAL_HALF_EXTENT
    horizontal_velocity = vx * SIMULATION_FPS / HORIZONTAL_HALF_EXTENT
    vertical_velocity = vy * SIMULATION_FPS / VERTICAL_HALF_EXTENT
    angular_velocity_radians_per_second = (
        angular_velocity * SIMULATION_FPS / ANGULAR_VELOCITY_SCALE
    )
    return FlightTelemetry(
        x_offset=x_offset,
        y_offset=y_offset,
        horizontal_velocity=horizontal_velocity,
        vertical_velocity=vertical_velocity,
        speed=math.hypot(horizontal_velocity, vertical_velocity),
        distance_from_pad=math.hypot(x_offset, y_offset),
        angle_degrees=math.degrees(angle),
        angular_velocity_radians_per_second=angular_velocity_radians_per_second,
        angular_velocity_degrees_per_second=math.degrees(angular_velocity_radians_per_second),
        left_leg_contact=left_contact >= 0.5,
        right_leg_contact=right_contact >= 0.5,
    )


def simulation_elapsed_time(steps: int) -> float:
    """Return simulated seconds at LunarLander's fixed 50 Hz update rate."""
    return steps / SIMULATION_FPS


def action_name(action: int) -> str:
    """Map a LunarLander discrete action to a readable engine name."""
    try:
        return ACTION_NAMES[int(action)]
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(f"Unknown LunarLander action: {action}") from error
