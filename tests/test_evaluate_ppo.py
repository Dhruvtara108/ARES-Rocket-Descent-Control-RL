"""Lightweight tests for PPO evaluation summary calculations."""

import unittest

from scripts.evaluate_ppo import EpisodeResult, calculate_summary


def result(reward: float, terminated: bool = False, truncated: bool = False) -> EpisodeResult:
    return EpisodeResult(
        episode=1,
        seed=100,
        total_reward=reward,
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

    def test_empty_results_are_rejected(self) -> None:
        with self.assertRaises(ValueError):
            calculate_summary([])


if __name__ == "__main__":
    unittest.main()
