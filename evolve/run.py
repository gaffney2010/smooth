#!/usr/bin/env python
"""Evolve smooth::EvolvedPlan with openevolve, then report the Pareto
frontier across bit_operations/scalar_operations/carries/total_converts/
atomic_transforms.

Usage (from the repo root, using the venv set up under evolve/.venv):

    evolve/.venv/bin/python evolve/run.py
    evolve/.venv/bin/python evolve/run.py --iterations 30   # quick smoke test
    evolve/.venv/bin/python evolve/run.py --output evolve/output2 --checkpoint evolve/output/checkpoints/checkpoint_50

Reads evolve/secrets.yaml for the API key (see evolve/secrets.yaml.example
for the format -- copy it to secrets.yaml and fill it in before running).
Everything else (model choice, population size, feature dimensions, the
domain-briefing system prompt) lives in evolve/config.yaml.
"""

import argparse
import sys
from pathlib import Path

import yaml

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from openevolve.api import run_evolution  # noqa: E402
from openevolve.config import Config, LLMModelConfig  # noqa: E402

import pareto  # noqa: E402


def load_secrets(path: Path) -> dict:
    if not path.exists():
        sys.exit(
            f"Missing {path}.\n"
            f"Copy evolve/secrets.yaml.example to evolve/secrets.yaml and fill in your API key."
        )
    with open(path) as f:
        data = yaml.safe_load(f) or {}
    if not data.get("api_key"):
        sys.exit(f"{path} has no 'api_key' entry -- see evolve/secrets.yaml.example.")
    return data


def build_config(secrets: dict) -> Config:
    config = Config.from_yaml(HERE / "config.yaml")

    config.llm.api_key = secrets["api_key"]
    if secrets.get("api_base"):
        config.llm.api_base = secrets["api_base"]
    if secrets.get("models"):
        config.llm.models = [
            LLMModelConfig(name=m["name"], weight=m.get("weight", 1.0)) for m in secrets["models"]
        ]

    # config.yaml's own models were already stamped with its api_base/None
    # api_key at parse time (LLMConfig.__post_init__); anything secrets.yaml
    # supplies only exists now, so push it down onto every model explicitly.
    config.llm.update_model_params(
        {"api_key": config.llm.api_key, "api_base": config.llm.api_base}, overwrite=True
    )
    return config


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--iterations", type=int, default=None, help="Overrides config.yaml's max_iterations")
    parser.add_argument("--output", default=str(HERE / "output"), help="Where checkpoints/results go")
    parser.add_argument("--checkpoint", default=None, help="Resume from a previous checkpoint directory")
    args = parser.parse_args()

    secrets = load_secrets(HERE / "secrets.yaml")
    config = build_config(secrets)

    print(f"Running with model(s): {[(m.name, m.weight) for m in config.llm.models]}")
    print(f"Output directory: {args.output}\n")

    result = run_evolution(
        initial_program=str(HERE / "initial_program.hpp"),
        evaluator=str(HERE / "evaluator.py"),
        config=config,
        iterations=args.iterations,
        output_dir=args.output,
        cleanup=False,
        checkpoint_path=args.checkpoint,
    )

    print(f"\nBest combined_score: {result.best_score:.2f}")
    print("Best program metrics:")
    for k, v in sorted(result.metrics.items()):
        print(f"  {k}: {v}")

    frontier = pareto.compute_and_report(args.output)
    print(f"\nPareto frontier: {len(frontier)} program(s).")
    print(f"See {args.output}/pareto_frontier.md and {args.output}/pareto_frontier.json")
    print(f"Each frontier program's full code is under {args.output}/pareto_programs/")


if __name__ == "__main__":
    main()
