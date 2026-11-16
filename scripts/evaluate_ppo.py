"""Evaluate a trained PPO model on reproducibly seeded LunarLander episodes."""

import argparse
import csv
import statistics
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Sequence

import gymnasium as gym
from stable_baselines3 import PPO


DEFAULT_MODEL_PATH = Path("checkpoints/ppo/ppo_lunarlander_final.zip")
DEFAULT_CSV_PATH = Path("checkpoints/ppo/evaluation/evaluation_results.csv")


@dataclass(frozen=True)
class EpisodeResult:
    episode: int
    seed: int
    total_reward: float
    steps: int
    terminated: bool
    truncated: bool
    final_x_position: float
    final_y_position: float
    final_horizontal_velocity: float
    final_vertical_velocity: float
    final_angle: float
    final_angular_velocity: float
    left_leg_contact: bool
    right_leg_contact: bool
    reached_200: bool


@dataclass(frozen=True)
class EvaluationSummary:
    mean_reward: float
    median_reward: float
    standard_deviation: float
    minimum_reward: float
    maximum_reward: float
    episodes_reaching_200: int
    percent_reaching_200: float
    negative_reward_episodes: int
    terminated_episodes: int
    truncated_episodes: int


CSV_FIELDS = tuple(EpisodeResult.__dataclass_fields__)


def calculate_summary(results: Sequence[EpisodeResult]) -> EvaluationSummary:
    """Calculate population summary metrics for completed episode results."""
    if not results:
        raise ValueError("At least one episode result is required")

    rewards = [result.total_reward for result in results]
    reaching_200 = sum(result.reached_200 for result in results)
    return EvaluationSummary(
        mean_reward=statistics.fmean(rewards),
        median_reward=statistics.median(rewards),
        standard_deviation=statistics.pstdev(rewards),
        minimum_reward=min(rewards),
        maximum_reward=max(rewards),
        episodes_reaching_200=reaching_200,
        percent_reaching_200=100.0 * reaching_200 / len(results),
        negative_reward_episodes=sum(reward < 0 for reward in rewards),
        terminated_episodes=sum(result.terminated for result in results),
        truncated_episodes=sum(result.truncated for result in results),
    )


def evaluate(
    model_path: Path = DEFAULT_MODEL_PATH,
    episodes: int = 100,
    seed: int = 100,
    stochastic: bool = False,
    csv_path: Path = DEFAULT_CSV_PATH,
) -> tuple[list[EpisodeResult], EvaluationSummary]:
    """Load a model on CPU, evaluate episodes, and write their metrics to CSV."""
    if episodes < 1:
        raise ValueError("episodes must be at least 1")
    if not model_path.is_file():
        raise FileNotFoundError(
            f"PPO model file not found: {model_path}. Train a model first or pass --model-path."
        )
    if model_path.resolve() == csv_path.resolve():
        raise ValueError("CSV output path must not overwrite the PPO model file")

    model = PPO.load(str(model_path), device="cpu")
    model.set_random_seed(seed)
    env = gym.make("LunarLander-v3")
    results: list[EpisodeResult] = []
    try:
        for episode in range(1, episodes + 1):
            episode_seed = seed + episode - 1
            observation, _ = env.reset(seed=episode_seed)
            total_reward = 0.0
            steps = 0
            terminated = truncated = False

            while not (terminated or truncated):
                action, _ = model.predict(observation, deterministic=not stochastic)
                observation, reward, terminated, truncated, _ = env.step(action)
                total_reward += float(reward)
                steps += 1

            # LunarLander observation: x, y, vx, vy, angle, angular velocity,
            # left-leg contact, right-leg contact. Keep the final post-step state.
            x, y, vx, vy, angle, angular_velocity, left_contact, right_contact = map(float, observation)
            results.append(
                EpisodeResult(
                    episode=episode,
                    seed=episode_seed,
                    total_reward=total_reward,
                    steps=steps,
                    terminated=bool(terminated),
                    truncated=bool(truncated),
                    final_x_position=x,
                    final_y_position=y,
                    final_horizontal_velocity=vx,
                    final_vertical_velocity=vy,
                    final_angle=angle,
                    final_angular_velocity=angular_velocity,
                    left_leg_contact=left_contact >= 0.5,
                    right_leg_contact=right_contact >= 0.5,
                    reached_200=total_reward >= 200.0,
                )
            )
    finally:
        env.close()

    summary = calculate_summary(results)
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(asdict(result) for result in results)

    print_summary(summary, episodes)
    print(f"Episode results CSV: {csv_path}")
    return results, summary


def print_summary(summary: EvaluationSummary, episodes: int) -> None:
    print(f"Evaluation summary ({episodes} episodes)")
    print(f"Mean reward: {summary.mean_reward:.2f}")
    print(f"Median reward: {summary.median_reward:.2f}")
    print(f"Standard deviation: {summary.standard_deviation:.2f}")
    print(f"Minimum reward: {summary.minimum_reward:.2f}")
    print(f"Maximum reward: {summary.maximum_reward:.2f}")
    print(
        f"Episodes reaching reward >=200: {summary.episodes_reaching_200} "
        f"({summary.percent_reaching_200:.1f}%)"
    )
    print(f"Negative-reward episodes: {summary.negative_reward_episodes}")
    print(f"Terminated episodes: {summary.terminated_episodes}")
    print(f"Truncated episodes: {summary.truncated_episodes}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model-path", type=Path, default=DEFAULT_MODEL_PATH)
    parser.add_argument("--episodes", type=int, default=100)
    parser.add_argument("--seed", type=int, default=100)
    parser.add_argument("--stochastic", action="store_true", help="sample actions from the policy")
    parser.add_argument("--csv-path", type=Path, default=DEFAULT_CSV_PATH)
    args = parser.parse_args()

    if args.episodes < 1:
        parser.error("--episodes must be at least 1")
    try:
        evaluate(args.model_path, args.episodes, args.seed, args.stochastic, args.csv_path)
    except (FileNotFoundError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
