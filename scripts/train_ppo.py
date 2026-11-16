"""Train a CPU PPO agent on Gymnasium LunarLander-v3."""

import argparse
from dataclasses import replace
from pathlib import Path

import gymnasium as gym
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import BaseCallback, CheckpointCallback, CallbackList
from stable_baselines3.common.monitor import Monitor

from scripts.ppo_config import DEFAULT_CONFIG, PPOConfig


class TrainingProgressCallback(BaseCallback):
    """Print a compact progress line at roughly ten points in a run."""

    def __init__(self, total_timesteps: int, rollout_steps: int, verbose: int = 0) -> None:
        super().__init__(verbose=verbose)
        # PPO finishes a rollout before checking the requested timestep limit.
        self.target_timesteps = ((total_timesteps + rollout_steps - 1) // rollout_steps) * rollout_steps
        self.report_every = max(1, self.target_timesteps // 10)
        self.last_report = 0

    def _on_step(self) -> bool:
        completed = min(self.num_timesteps, self.target_timesteps)
        if self.last_report >= self.target_timesteps:
            return True
        if completed - self.last_report >= self.report_every or completed >= self.target_timesteps:
            progress = min(100.0, 100 * completed / self.target_timesteps)
            print(f"Training progress: {completed:,}/{self.target_timesteps:,} timesteps ({progress:.1f}%)")
            self.last_report = completed
        return True


def create_model(env: gym.Env, config: PPOConfig = DEFAULT_CONFIG) -> PPO:
    """Build PPO with the project's centralized hyperparameters."""
    return PPO(
        config.policy,
        env,
        learning_rate=config.learning_rate,
        n_steps=config.n_steps,
        batch_size=config.batch_size,
        gamma=config.gamma,
        gae_lambda=config.gae_lambda,
        clip_range=config.clip_range,
        ent_coef=config.ent_coef,
        vf_coef=config.vf_coef,
        max_grad_norm=config.max_grad_norm,
        policy_kwargs={"net_arch": list(config.hidden_layers)},
        seed=config.seed,
        device=config.device,
        verbose=1,
    )


def train(config: PPOConfig = DEFAULT_CONFIG) -> Path:
    """Train PPO, periodically checkpoint, save the final model, and close the env."""
    output_dir = Path(config.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    env = gym.make(config.env_id)
    try:
        env = Monitor(env, filename=str(output_dir / "monitor.csv"))
        env.reset(seed=config.seed)
        env.action_space.seed(config.seed)
        model = create_model(env, config)
        callbacks = CallbackList(
            [
                CheckpointCallback(
                    save_freq=config.checkpoint_freq,
                    save_path=str(output_dir),
                    name_prefix="ppo_lunarlander",
                    save_replay_buffer=False,
                    save_vecnormalize=False,
                ),
                TrainingProgressCallback(config.total_timesteps, config.n_steps),
            ]
        )
        model.learn(total_timesteps=config.total_timesteps, callback=callbacks, progress_bar=False)
        final_path = output_dir / "ppo_lunarlander_final"
        model.save(str(final_path))
        print(f"Saved final model: {final_path}.zip")
        return final_path.with_suffix(".zip")
    finally:
        env.close()


def parse_args() -> PPOConfig:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--total-timesteps", type=int, default=DEFAULT_CONFIG.total_timesteps)
    parser.add_argument("--seed", type=int, default=DEFAULT_CONFIG.seed)
    parser.add_argument("--checkpoint-freq", type=int, default=DEFAULT_CONFIG.checkpoint_freq)
    parser.add_argument("--learning-rate", type=float, default=DEFAULT_CONFIG.learning_rate)
    parser.add_argument("--batch-size", type=int, default=DEFAULT_CONFIG.batch_size)
    parser.add_argument("--n-steps", type=int, default=DEFAULT_CONFIG.n_steps)
    parser.add_argument("--output-dir", type=Path, default=Path(DEFAULT_CONFIG.output_dir))
    args = parser.parse_args()
    return replace(
        DEFAULT_CONFIG,
        total_timesteps=args.total_timesteps,
        seed=args.seed,
        checkpoint_freq=args.checkpoint_freq,
        learning_rate=args.learning_rate,
        batch_size=args.batch_size,
        n_steps=args.n_steps,
        output_dir=str(args.output_dir),
    )


def main() -> None:
    train(parse_args())


if __name__ == "__main__":
    main()
