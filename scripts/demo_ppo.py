"""Demonstrate a trained PPO policy in Gymnasium LunarLander-v3."""

import argparse
from pathlib import Path
import time

import gymnasium as gym
from stable_baselines3 import PPO

from scripts.evaluate_ppo import classify_termination_outcome
from scripts.telemetry import (
    ACTION_NAMES,
    action_name,
    simulation_elapsed_time,
    telemetry_from_observation,
)


DEFAULT_MODEL_PATH = Path("checkpoints/ppo/ppo_lunarlander_final.zip")


def _draw_hud(
    action: int,
    action_counts: dict[int, int],
    elapsed_real: float,
    episode: int,
    max_steps: int,
    observation,
    reward: float,
    seed: int,
    steps: int,
    total_reward: float,
    terminated: bool,
    truncated: bool,
) -> None:
    """Draw a translucent telemetry panel over the active Gymnasium display."""
    # Keep Pygame out of imports and initialization unless graphical rendering is requested.
    import pygame

    surface = pygame.display.get_surface()
    if surface is None:
        return
    metrics = telemetry_from_observation(observation)
    action_label = action_name(action)
    firing = "OFF" if action == 0 else "ON"
    if terminated or truncated:
        status = classify_termination_outcome(terminated, truncated, reward)
    elif metrics.left_leg_contact or metrics.right_leg_contact:
        status = "LEG CONTACT"
    else:
        status = "AIRBORNE"

    rows = [
        (f"EPISODE {episode}  |  SEED {seed}", ""),
        (f"Step {steps} / {max_steps}", f"Sim {simulation_elapsed_time(steps):.2f}s"),
        ("Real elapsed", f"{elapsed_real:.2f}s"),
        ("POSITION (sim units)", ""),
        ("X offset from pad", f"{metrics.x_offset:+.2f}"),
        ("Y from pad reference", f"{metrics.y_offset:+.2f}"),
        ("Distance from pad", f"{metrics.distance_from_pad:.2f}"),
        ("VELOCITY (sim units/s)", ""),
        ("Horizontal", f"{metrics.horizontal_velocity:+.2f}"),
        ("Vertical", f"{metrics.vertical_velocity:+.2f}"),
        ("Total speed", f"{metrics.speed:.2f}"),
        ("ORIENTATION", ""),
        ("Angle", f"{metrics.angle_degrees:+.1f} deg"),
        ("Angular velocity", f"{metrics.angular_velocity_radians_per_second:+.2f} rad/s"),
        ("CONTROL", ""),
        ("Action / thruster", f"{action_label} / {firing}"),
        ("Action counts C/L/M/R", " / ".join(str(action_counts[a]) for a in range(4))),
        ("LANDING", ""),
        ("Leg contact L/R", f"{metrics.left_leg_contact} / {metrics.right_leg_contact}"),
        ("Reward", f"{total_reward:+.1f}"),
        ("Status", status),
    ]
    font = pygame.font.Font(None, 19)
    line_height = 17
    panel_width = min(348, surface.get_width() - 16)
    panel_height = line_height * len(rows) + 14
    panel = pygame.Surface((panel_width, panel_height), pygame.SRCALPHA)
    panel.fill((14, 24, 38, 205))
    for index, (label, value) in enumerate(rows):
        color = (114, 205, 255) if not value else (240, 245, 250)
        left = font.render(label, True, color)
        panel.blit(left, (9, 7 + index * line_height))
        if value:
            right = font.render(value, True, (255, 218, 130))
            panel.blit(right, (panel_width - right.get_width() - 9, 7 + index * line_height))
    # Upper-left placement stays away from the centered pad and the usual flight path.
    surface.blit(panel, (8, 8))
    pygame.display.flip()


def _print_flight_report(
    action_counts: dict[int, int],
    elapsed_real: float,
    episode: int,
    final_observation,
    final_step_reward: float,
    seed: int,
    steps: int,
    total_reward: float,
    terminated: bool,
    truncated: bool,
) -> None:
    metrics = telemetry_from_observation(final_observation)
    outcome = classify_termination_outcome(terminated, truncated, final_step_reward)
    print(f"Episode {episode} flight report (seed {seed})")
    print(f"  Reward: {total_reward:.2f}; terminal outcome: {outcome}")
    print(
        f"  Duration: simulation {simulation_elapsed_time(steps):.2f}s, "
        f"real {elapsed_real:.2f}s; steps {steps}"
    )
    print(f"  Final position: X {metrics.x_offset:+.3f}, Y {metrics.y_offset:+.3f} simulation units")
    print(
        f"  Final velocity: horizontal {metrics.horizontal_velocity:+.3f}, "
        f"vertical {metrics.vertical_velocity:+.3f}, total {metrics.speed:.3f} simulation units/s"
    )
    print(
        f"  Pad distance: {metrics.distance_from_pad:.3f} simulation units; "
        f"angle {metrics.angle_degrees:+.2f} deg; "
        f"angular velocity {metrics.angular_velocity_radians_per_second:+.3f} rad/s"
    )
    print(f"  Leg contact: left {metrics.left_leg_contact}, right {metrics.right_leg_contact}")
    print(
        "  Action counts (coast, left, main, right): "
        + ", ".join(str(action_counts[action]) for action in range(4))
    )
    if outcome == "RESTED":
        print("  Note: RESTED identifies a terminal reward event, not pad-landing success.")


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

    pygame_module = None
    if render:
        # Gymnasium and the HUD share this display; non-graphical runs never import Pygame.
        import pygame as pygame_module

        pygame_module.init()

    env = None
    rewards: list[float] = []
    try:
        model = PPO.load(str(model_path), device="cpu")
        env = gym.make("LunarLander-v3", render_mode="human" if render else None)
        max_steps = int(env.spec.max_episode_steps or 1000)
        for episode in range(1, episodes + 1):
            episode_seed = seed + episode - 1
            episode_start = time.perf_counter()
            observation, _ = env.reset(seed=episode_seed)
            total_reward = 0.0
            final_step_reward = 0.0
            steps = 0
            action_counts = {action: 0 for action in ACTION_NAMES}
            terminated = truncated = False

            while not (terminated or truncated):
                action, _ = model.predict(observation, deterministic=not stochastic)
                action_index = int(action)
                action_counts[action_index] += 1
                observation, reward, terminated, truncated, _ = env.step(action)
                final_step_reward = float(reward)
                total_reward += final_step_reward
                steps += 1
                if render:
                    _draw_hud(
                        action_index,
                        action_counts,
                        time.perf_counter() - episode_start,
                        episode,
                        max_steps,
                        observation,
                        final_step_reward,
                        episode_seed,
                        steps,
                        total_reward,
                        bool(terminated),
                        bool(truncated),
                    )

            elapsed_real = time.perf_counter() - episode_start
            rewards.append(total_reward)
            _print_flight_report(
                action_counts,
                elapsed_real,
                episode,
                observation,
                final_step_reward,
                episode_seed,
                steps,
                total_reward,
                bool(terminated),
                bool(truncated),
            )

        print(f"Average reward: {sum(rewards) / len(rewards):.2f}")
        return rewards
    finally:
        if env is not None:
            env.close()
        if pygame_module is not None:
            pygame_module.quit()


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
