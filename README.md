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
implemented. Phase 2.1 provides PPO configuration and the training pipeline;
model training/checkpoint evaluation and model loading/demonstration remain later
Phase 2 work.
