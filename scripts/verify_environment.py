"""Check required dependencies and the Gymnasium LunarLander-v3 API."""

import gymnasium as gym
import numpy as np
import torch
import stable_baselines3
import Box2D


def main() -> None:
    print(f"PyTorch: {torch.__version__}")
    print(f"NumPy: {np.__version__}")
    print(f"Gymnasium: {gym.__version__}")
    print(f"Stable-Baselines3: {stable_baselines3.__version__}")
    print(f"Box2D: {getattr(Box2D, '__version__', 'importable')}")

    env = gym.make("LunarLander-v3")
    try:
        observation, info = env.reset()
        assert observation.shape == (8,), f"Unexpected observation shape: {observation.shape}"
        assert isinstance(env.action_space, gym.spaces.Discrete)
        assert env.action_space.n == 4, f"Expected 4 actions, got {env.action_space.n}"

        action = env.action_space.sample()
        assert env.action_space.contains(action)
        next_observation, reward, terminated, truncated, step_info = env.step(action)
        assert next_observation.shape == (8,)
        assert np.isfinite(next_observation).all(), "Step observation contains non-finite values"
        assert np.isfinite(reward), f"Non-finite reward: {reward}"
        print(f"Environment check passed; action={action}, reward={reward:.3f}")
    finally:
        env.close()


if __name__ == "__main__":
    main()
