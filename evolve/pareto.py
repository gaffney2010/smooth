"""Extract the Pareto-optimal set of evolved smooth::EvolvedPlan candidates
from an openevolve output directory, across the five scored metrics
(bit_operations, scalar_operations, carries, total_converts,
atomic_transforms) -- all minimized.

openevolve's own MAP-Elites database already keeps one good candidate per
region of this 5D cost space (that's what config.yaml's feature_dimensions
does), but it doesn't compute or report the actual Pareto front. This reads
every program openevolve ever checkpointed, throws out anything that didn't
compile/run/match DefaultPlan (see evaluator.py's "correct" metric), and
keeps only the non-dominated ones.

Usable standalone: `python pareto.py [output_dir]`.
"""

import json
import sys
from pathlib import Path
from typing import Dict, List

METRICS = ["bit_operations", "scalar_operations", "carries", "total_converts", "atomic_transforms"]


def _iteration_of(program: dict) -> int:
    return program.get("iteration_found", program.get("iteration", 0)) or 0


def _load_programs(output_dir: str) -> Dict[str, dict]:
    """Every program from every checkpoint, deduped by id, plus the final
    best/ record as a fallback for whichever trailing iterations landed
    after the last checkpoint_interval boundary (see controller.py:
    _run_evolution_with_checkpoints only saves a final checkpoint when the
    iteration count divides evenly)."""
    programs: Dict[str, dict] = {}
    out = Path(output_dir)

    checkpoints_dir = out / "checkpoints"
    for checkpoint_dir in sorted(checkpoints_dir.glob("checkpoint_*")):
        programs_dir = checkpoint_dir / "programs"
        if not programs_dir.exists():
            continue
        for program_file in programs_dir.glob("*.json"):
            try:
                data = json.loads(program_file.read_text())
            except Exception:
                continue
            programs[data["id"]] = data

    best_info_path = out / "best" / "best_program_info.json"
    best_code_path = out / "best" / "best_program.hpp"
    if best_info_path.exists():
        try:
            data = dict(json.loads(best_info_path.read_text()))
            if best_code_path.exists():
                data["code"] = best_code_path.read_text()
            programs[data["id"]] = data
        except Exception:
            pass

    return programs


def _is_valid(metrics: dict) -> bool:
    if metrics.get("correct", 1.0) < 1.0:
        return False
    return all(name in metrics for name in METRICS)


def dominates(a: dict, b: dict) -> bool:
    """True if metrics `a` dominates metrics `b`: at least as good on every
    tracked metric, strictly better on at least one. Lower is better on all
    five (they're all costs)."""
    not_worse = all(a[m] <= b[m] for m in METRICS)
    strictly_better = any(a[m] < b[m] for m in METRICS)
    return not_worse and strictly_better


def pareto_front(programs: Dict[str, dict]) -> List[dict]:
    candidates = [p for p in programs.values() if _is_valid(p.get("metrics", {}))]

    frontier = []
    for candidate in candidates:
        if any(
            other is not candidate and dominates(other["metrics"], candidate["metrics"])
            for other in candidates
        ):
            continue
        frontier.append(candidate)

    # Collapse candidates that landed on the exact same metric tuple
    # (common early on, before evolution diversifies) to the earliest one.
    seen = {}
    for p in sorted(frontier, key=_iteration_of):
        key = tuple(p["metrics"][m] for m in METRICS)
        seen.setdefault(key, p)

    return sorted(seen.values(), key=lambda p: sum(p["metrics"][m] for m in METRICS))


def compute_and_report(output_dir: str) -> List[dict]:
    programs = _load_programs(output_dir)
    frontier = pareto_front(programs)

    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)

    (out / "pareto_frontier.json").write_text(
        json.dumps(
            [
                {
                    "id": p["id"],
                    "iteration_found": _iteration_of(p),
                    "metrics": {m: p["metrics"][m] for m in METRICS},
                    "combined_score": p["metrics"].get("combined_score"),
                }
                for p in frontier
            ],
            indent=2,
        )
    )

    lines = [
        "# Pareto frontier",
        "",
        f"{len(programs)} program(s) evaluated in total; {len(frontier)} are Pareto-optimal "
        "across bit_operations / scalar_operations / carries / total_converts / "
        "atomic_transforms (all minimized -- none of these beats another frontier member on "
        "every axis at once).",
        "",
    ]
    header = ["id", "iteration"] + METRICS
    lines.append("| " + " | ".join(header) + " |")
    lines.append("|" + "|".join(["---"] * len(header)) + "|")
    for p in frontier:
        row = [p["id"][:8], str(_iteration_of(p))] + [str(int(p["metrics"][m])) for m in METRICS]
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")

    programs_out = out / "pareto_programs"
    programs_out.mkdir(exist_ok=True)
    for p in frontier:
        code = p.get("code")
        if not code:
            continue
        filename = f"{p['id'][:8]}.hpp"
        (programs_out / filename).write_text(code)
        lines.append(f"## {p['id'][:8]} (iteration {_iteration_of(p)})")
        lines.append("")
        lines.append(f"Full code: `pareto_programs/{filename}`")
        lines.append("")
        lines.append("```cpp")
        lines.append(code)
        lines.append("```")
        lines.append("")

    (out / "pareto_frontier.md").write_text("\n".join(lines))

    return frontier


if __name__ == "__main__":
    output_dir = sys.argv[1] if len(sys.argv) > 1 else str(Path(__file__).resolve().parent / "output")
    result = compute_and_report(output_dir)
    print(f"{len(result)} Pareto-optimal program(s); see {output_dir}/pareto_frontier.md")
