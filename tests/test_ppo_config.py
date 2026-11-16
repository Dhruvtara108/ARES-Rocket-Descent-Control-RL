"""Configuration and model construction checks for PPO training."""

import unittest

import gymnasium as gym
from stable_baselines3 import PPO

from scripts.ppo_config import DEFAULT_CONFIG, PPOConfig
from scripts.train_ppo import create_model


class PPOConfigurationTests(unittest.TestCase):
    def test_requested_hyperparameters_are_centralized(self) -> None:
        config = DEFAULT_CONFIG
        self.assertEqual(config.env_id, "LunarLander-v3")
        self.assertEqual(config.policy, "MlpPolicy")
        self.assertEqual(config.algorithm, "PPO")
        self.assertEqual(config.device, "cpu")
        self.assertEqual(config.learning_rate, 0.0003)
        self.assertEqual(config.n_steps, 2048)
        self.assertEqual(config.batch_size, 64)
        self.assertEqual(config.gamma, 0.99)
        self.assertEqual(config.gae_lambda, 0.95)
        self.assertEqual(config.clip_range, 0.2)
        self.assertEqual(config.ent_coef, 0.0)
        self.assertEqual(config.vf_coef, 0.5)
        self.assertEqual(config.max_grad_norm, 0.5)
        self.assertEqual(config.hidden_layers, (64, 64))

    def test_invalid_run_sizes_are_rejected(self) -> None:
        with self.assertRaises(ValueError):
            PPOConfig(total_timesteps=0)
        with self.assertRaises(ValueError):
            PPOConfig(checkpoint_freq=0)

    def test_ppo_initializes_with_compatible_spaces_and_cpu_policy(self) -> None:
        env = gym.make(DEFAULT_CONFIG.env_id)
        try:
            model = create_model(env)
            self.assertIsInstance(model, PPO)
            self.assertEqual(model.device.type, "cpu")
            self.assertEqual(model.observation_space.shape, (8,))
            self.assertEqual(model.action_space, gym.spaces.Discrete(4))
            self.assertEqual(model.policy.mlp_extractor.policy_net[0].out_features, 64)
            self.assertEqual(model.policy.mlp_extractor.policy_net[2].out_features, 64)
        finally:
            env.close()


if __name__ == "__main__":
    unittest.main()
