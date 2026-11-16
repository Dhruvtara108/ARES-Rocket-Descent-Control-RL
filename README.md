# ARES Rocket Descent Control RL

Phase 1 establishes a reproducible LunarLander simulation and random-agent baseline. No learned policy or PPO training is included yet.

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

## Phase 1 status

Environment dependency checks, API verification, lightweight tests, and a random-action evaluation are implemented. Phase 1 is complete when these checks pass in the project environment. Training or optimizing a policy is outside this phase.
