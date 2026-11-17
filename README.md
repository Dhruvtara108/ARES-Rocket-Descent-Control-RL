# ARES Rocket Descent Control RL

This project trains an autonomous LunarLander agent with Proximal Policy Optimization (PPO) from Stable-Baselines3. Phase 1 provides the simulation checks and random-agent baseline; Phase 2.1 adds the PPO configuration and training pipeline.

## Environment setup

Use Python 3.12 and a CPU-only PyTorch installation. From the repository root:

```bash
python3.12 -m venv .venv
source .venv/bin/activate
python -m pip install torch==2.15.0 --index-url https://download.pytorch.org/whl/cpu
python -m pip install -r requirements.txt
```

PyTorch is installed separately from the CPU-only wheel index. It is intentionally
omitted from `requirements.txt` so a later requirements install cannot replace it
with a non-CPU build. The remaining pinned dependencies are NumPy, Gymnasium,
Stable-Baselines3, and Box2D. `LunarLander-v3` uses Gymnasium's Box2D environment.

## PPO training (Phase 2.1)

PPO learns a policy by collecting short batches of environment experience and
updating the policy while limiting each update to avoid large, unstable changes.
It is used here as a well-supported on-policy baseline for the discrete actions
and continuous observations in LunarLander.

The policy is `MlpPolicy` with separate policy and value networks, each using two
hidden layers of 64 units. Training runs on CPU. The centralized defaults in
`scripts/ppo_config.py` are:

| Setting | Value |
| --- | ---: |
| Environment / algorithm | `LunarLander-v3` / PPO |
| Learning rate | 0.0003 |
| Rollout steps | 2048 |
| Minibatch size | 64 |
| Gamma / GAE lambda | 0.99 / 0.95 |
| Clip range | 0.2 |
| Entropy / value coefficients | 0.0 / 0.5 |
| Maximum gradient norm | 0.5 |
| Hidden layers | 64, 64 |
| Default total timesteps / seed | 500,000 / 42 |
| Checkpoint frequency | 100,000 timesteps |

Start training from the repository root with the existing virtual environment:

```bash
source .venv/bin/activate
python -m scripts.train_ppo
```

The run length, seed, checkpoint interval, main PPO settings, and output directory
can be overridden. For example:

```bash
python -m scripts.train_ppo --total-timesteps 100000 --seed 7 --checkpoint-freq 25000
```

Periodic checkpoints, the final model, and a compact episode monitor log are
written under `checkpoints/ppo/`, which is ignored by Git. Model files are only
created when training is run; this Phase 2.1 change does not launch a full run.

## Model demonstration (Phase 2.3)

The demonstration script loads the locally trained PPO model on CPU and runs it
in LunarLander-v3. Actions are deterministic by default. Add `--stochastic` to
sample actions from the policy instead.

Run one episode with the graphical environment:

```bash
python -m scripts.demo_ppo --render
```

Run three episodes without rendering:

```bash
python -m scripts.demo_ppo --episodes 3
```

The script defaults to `checkpoints/ppo/ppo_lunarlander_final.zip`. Select a
different model, episode count, or seed with `--model-path`, `--episodes`, or
`--seed`. The model is generated locally by training and is not committed to Git;
run Phase 2.1 training first if the default model file is missing.

## Detailed evaluation (Phase 3.1)

Evaluate the trained policy over 100 seeded episodes and write per-episode
rewards, termination flags, and final landing observations to CSV:

```bash
python -m scripts.evaluate_ppo
```

The default seeds are 100–199 and the CSV is written to
`checkpoints/ppo/evaluation/evaluation_results.csv`. Use `--episodes`, `--seed`,
`--model-path`, `--csv-path`, or `--stochastic` to customize the run. Reward of
200 or more is counted separately as a score threshold; termination alone does
not indicate a successful landing. Final position, velocity, angle, and leg
contact values are taken from LunarLander's final eight-value observation; its
position and velocity values retain Gymnasium's observation scaling.

The evaluation also records the reward from the final environment step and
classifies the episode-ending event: `RESTED` means a terminated episode ended
with a +100 final-step reward, `CRASHED_OR_OUT_OF_BOUNDS` means termination
with a -100 final-step reward, `TIME_LIMIT` means the episode was truncated,
and `OTHER` covers any other ending. `RESTED` describes only the environment's
terminal reward event; it does not by itself confirm that the lander reached the
designated landing pad or that the landing was successful.

## Verify the simulation

```bash
python scripts/verify_environment.py
python -m unittest discover -s tests -v
```

The verification script reports dependency versions, creates and resets the environment, checks the 8-value observation and four-action discrete space, and performs one valid step.

## Random-agent baseline

Run five episodes by default, or choose a count:

```bash
python scripts/random_agent.py --episodes 10
```

Each episode's reward and length are printed, followed by the average reward. To show a human-rendered simulation window:

```bash
python scripts/random_agent.py --episodes 3 --render
```

Rendering requires a graphical display. Generated outputs are not written to the repository.

## Phase status

Phase 1 environment checks, lightweight tests, and random-agent evaluation are
implemented. Phase 2.1 provides PPO configuration and the training pipeline,
and Phase 2.3 provides model loading and demonstration. Phase 3.1 adds detailed
evaluation metrics; further training evaluation and optimization remain future
work.
