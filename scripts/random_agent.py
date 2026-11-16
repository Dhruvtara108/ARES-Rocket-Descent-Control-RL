"""Evaluate a random policy on Gymnasium's LunarLander-v3."""

import argparse

import gymnasium as gym


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--episodes", type=int, default=5, help="number of episodes (default: 5)")
    parser.add_argument("--render", action="store_true", help="show the simulation window")
    args = parser.parse_args()
    if args.episodes < 1:
        parser.error("--episodes must be at least 1")

    env = gym.make("LunarLander-v3", render_mode="human" if args.render else None)
    rewards: list[float] = []
    lengths: list[int] = []
    try:
        for episode in range(1, args.episodes + 1):
            observation, info = env.reset()
            total_reward = 0.0
            length = 0
            terminated = truncated = False
            while not (terminated or truncated):
                action = env.action_space.sample()
                observation, reward, terminated, truncated, info = env.step(action)
                total_reward += float(reward)
                length += 1
            rewards.append(total_reward)
            lengths.append(length)
            print(f"Episode {episode}: reward={total_reward:.2f}, length={length}")

        print(f"Average reward: {sum(rewards) / len(rewards):.2f}")
    finally:
        env.close()


if __name__ == "__main__":
    main()
