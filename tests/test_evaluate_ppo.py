"""Lightweight tests for PPO evaluation summary calculations."""

import unittest

from scripts.evaluate_ppo import (
    CRASHED_OR_OUT_OF_BOUNDS,
    OTHER,
    RESTED,
    TIME_LIMIT,
    EpisodeResult,
    calculate_summary,
    classify_termination_outcome,
)


def result(reward: float, terminated: bool = False, truncated: bool = False) -> EpisodeResult:
    return EpisodeResult(
        episode=1,
        seed=100,
        total_reward=reward,
        final_step_reward=reward,
        termination_outcome=classify_termination_outcome(terminated, truncated, reward),
        steps=10,
        terminated=terminated,
        truncated=truncated,
        final_x_position=0.0,
        final_y_position=0.0,
        final_horizontal_velocity=0.0,
        final_vertical_velocity=0.0,
        final_angle=0.0,
        final_angular_velocity=0.0,
        left_leg_contact=False,
        right_leg_contact=False,
        reached_200=reward >= 200,
        final_x_offset_sim_units=0.0,
        final_y_offset_sim_units=0.0,
        final_horizontal_velocity_sim_units_per_second=0.0,
        final_vertical_velocity_sim_units_per_second=0.0,
        final_speed_sim_units_per_second=0.0,
        final_distance_from_pad_sim_units=0.0,
        final_angle_degrees=0.0,
        final_angular_velocity_radians_per_second=0.0,
        simulation_elapsed_time_seconds=0.2,
        real_elapsed_evaluation_time_seconds=0.1,
    )


class EvaluationStatisticsTests(unittest.TestCase):
    def test_calculates_reward_and_episode_counts(self) -> None:
        summary = calculate_summary(
            [
                result(-10, terminated=True),
                result(200, truncated=True),
                result(10),
            ]
        )
        self.assertEqual(summary.mean_reward, 200 / 3)
        self.assertEqual(summary.median_reward, 10)
        self.assertAlmostEqual(summary.standard_deviation, 94.63, places=2)
        self.assertEqual(summary.minimum_reward, -10)
        self.assertEqual(summary.maximum_reward, 200)
        self.assertEqual(summary.episodes_reaching_200, 1)
        self.assertAlmostEqual(summary.percent_reaching_200, 100 / 3)
        self.assertEqual(summary.negative_reward_episodes, 1)
        self.assertEqual(summary.terminated_episodes, 1)
        self.assertEqual(summary.truncated_episodes, 1)
        self.assertEqual(summary.rested_episodes, 0)
        self.assertEqual(summary.crashed_or_out_of_bounds_episodes, 0)
        self.assertEqual(summary.time_limit_episodes, 1)
        self.assertEqual(summary.other_outcome_episodes, 2)

    def test_classifies_rested(self) -> None:
        self.assertEqual(classify_termination_outcome(True, False, 100.0), RESTED)

    def test_classifies_crash_or_out_of_bounds(self) -> None:
        self.assertEqual(
            classify_termination_outcome(True, False, -100.0), CRASHED_OR_OUT_OF_BOUNDS
        )

    def test_classifies_time_limit(self) -> None:
        self.assertEqual(classify_termination_outcome(False, True, 0.0), TIME_LIMIT)

    def test_classifies_unexpected_termination(self) -> None:
        self.assertEqual(classify_termination_outcome(True, False, 17.5), OTHER)

    def test_empty_results_are_rejected(self) -> None:
        with self.assertRaises(ValueError):
            calculate_summary([])


if __name__ == "__main__":
    unittest.main()
