"""Central configuration for PPO LunarLander training."""

from dataclasses import dataclass


@dataclass(frozen=True)
class PPOConfig:
    env_id: str = "LunarLander-v3"
    policy: str = "MlpPolicy"
    algorithm: str = "PPO"
    device: str = "cpu"
    total_timesteps: int = 500_000
    learning_rate: float = 0.0003
    n_steps: int = 2048
    batch_size: int = 64
    gamma: float = 0.99
    gae_lambda: float = 0.95
    clip_range: float = 0.2
    ent_coef: float = 0.0
    vf_coef: float = 0.5
    max_grad_norm: float = 0.5
    hidden_layers: tuple[int, int] = (64, 64)
    seed: int = 42
    checkpoint_freq: int = 100_000
    output_dir: str = "checkpoints/ppo"

    def __post_init__(self) -> None:
        if self.total_timesteps < 1:
            raise ValueError("total_timesteps must be at least 1")
        if self.checkpoint_freq < 1:
            raise ValueError("checkpoint_freq must be at least 1")
        if self.batch_size < 1 or self.n_steps < 1:
            raise ValueError("batch_size and n_steps must be at least 1")


DEFAULT_CONFIG = PPOConfig()
