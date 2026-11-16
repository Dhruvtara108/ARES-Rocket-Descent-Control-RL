"""Demonstrate a trained PPO policy in Gymnasium LunarLander-v3."""

import argparse
from pathlib import Path

import gymnasium as gym
from stable_baselines3 import PPO


DEFAULT_MODEL_PATH = Path("checkpoints/ppo/ppo_lunarlander_final.zip")


def demonstrate(
    model_path: Path,
    episodes: int = 1,
    seed: int = 42,
    render: bool = False,
    stochastic: bool = False,
) -> list[float]:
    """Load a CPU PPO model and run seeded demonstration episodes."""
    if episodes < 1:
        raise ValueError("episodes must be at least 1")
    if not model_path.is_file():
        raise FileNotFoundError(
            f"PPO model file not found: {model_path}. Train a model first or pass --model-path."
        )

    model = PPO.load(str(model_path), device="cpu")
    env = gym.make("LunarLander-v3", render_mode="human" if render else None)
    rewards: list[float] = []
    try:
        for episode in range(1, episodes + 1):
            observation, _ = env.reset(seed=seed + episode - 1)
            total_reward = 0.0
            steps = 0
            terminated = truncated = False

            while not (terminated or truncated):
                action, _ = model.predict(observation, deterministic=not stochastic)
                observation, reward, terminated, truncated, _ = env.step(action)
                total_reward += float(reward)
                steps += 1

            rewards.append(total_reward)
            print(
                f"Episode {episode}: reward={total_reward:.2f}, steps={steps}, "
                f"terminated={terminated}, truncated={truncated}"
            )

        print(f"Average reward: {sum(rewards) / len(rewards):.2f}")
        return rewards
    finally:
        env.close()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model-path", type=Path, default=DEFAULT_MODEL_PATH)
    parser.add_argument("--episodes", type=int, default=1)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--render", action="store_true", help="show the simulation window")
    parser.add_argument(
        "--stochastic", action="store_true", help="sample actions instead of choosing the most likely action"
    )
    args = parser.parse_args()

    if args.episodes < 1:
        parser.error("--episodes must be at least 1")
    try:
        demonstrate(args.model_path, args.episodes, args.seed, args.render, args.stochastic)
    except FileNotFoundError as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
