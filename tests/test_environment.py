"""Lightweight checks for dependencies and the LunarLander API."""

import unittest

import Box2D
import gymnasium as gym
import numpy as np
import stable_baselines3
import torch


class LunarLanderTests(unittest.TestCase):
    def setUp(self) -> None:
        self.env = gym.make("LunarLander-v3")

    def tearDown(self) -> None:
        self.env.close()

    def test_environment_creation(self) -> None:
        self.assertIsNotNone(self.env.unwrapped)
        self.assertEqual(self.env.spec.id, "LunarLander-v3")

    def test_observation_dimensions(self) -> None:
        observation, _ = self.env.reset()
        self.assertEqual(observation.shape, (8,))

    def test_action_space(self) -> None:
        self.assertIsInstance(self.env.action_space, gym.spaces.Discrete)
        self.assertEqual(self.env.action_space.n, 4)
        self.assertTrue(self.env.action_space.contains(self.env.action_space.sample()))

    def test_reset_and_step_api(self) -> None:
        observation, info = self.env.reset()
        self.assertEqual(observation.shape, (8,))
        self.assertIsInstance(info, dict)
        result = self.env.step(self.env.action_space.sample())
        next_observation, reward, terminated, truncated, info = result
        self.assertEqual(next_observation.shape, (8,))
        self.assertTrue(np.isfinite(next_observation).all())
        self.assertTrue(np.isfinite(reward))
        self.assertIsInstance(terminated, bool)
        self.assertIsInstance(truncated, bool)
        self.assertIsInstance(info, dict)

    def test_dependency_imports(self) -> None:
        self.assertTrue(torch.__version__)
        self.assertTrue(np.__version__)
        self.assertTrue(gym.__version__)
        self.assertTrue(stable_baselines3.__version__)
        self.assertIsNotNone(Box2D)


if __name__ == "__main__":
    unittest.main()
