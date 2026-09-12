"""openevolve evaluator for smooth::EvolvedPlan.

For each candidate program (a full replacement for the EVOLVE-BLOCK in
initial_program.hpp, defining smooth::EvolvedPlan), this:

  1. Drops the candidate in as evolved_plan.hpp in a scratch build dir.
  2. Compiles evolve/harness.cpp against it and the library's include/.
  3. Runs the harness, which checks EvolvedPlan against DefaultPlan for
     correctness on every profile in smooth::allProfiles() and sums its
     Metrics counters across all of them.
  4. Turns those counters into the metrics dict openevolve scores on.

A program that fails to compile, crashes, or produces wrong results gets a
very negative combined_score plus a "why" recorded in the artifacts side
channel (openevolve feeds that back to the LLM on the next prompt), so
evolution can course-correct instead of just seeing a low number.
"""

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from openevolve.evaluation_result import EvaluationResult

REPO_ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIR = REPO_ROOT / "include"
HARNESS_CPP = Path(__file__).resolve().parent / "harness.cpp"
CXX = os.environ.get("SMOOTH_EVOLVE_CXX", "c++")

# The five counters we're actually trying to shrink -- see evolve/README.md
# for what each one means and why it costs what it costs.
TRACKED_METRICS = [
    "bit_operations",
    "scalar_operations",
    "carries",
    "total_converts",
    "atomic_transforms",
]

COMPILE_TIMEOUT_S = 60
RUN_TIMEOUT_S = 30

# A generous ceiling used to penalize a broken candidate on every tracked
# metric (rather than leaving them at 0, which would make "doesn't compile"
# look identical to "found a free lunch" on the MAP-Elites feature grid).
FAILURE_METRIC_VALUE = 1_000_000.0


def _failure(reason: str, log: str = "") -> EvaluationResult:
    metrics = {name: FAILURE_METRIC_VALUE for name in TRACKED_METRICS}
    metrics["combined_score"] = -1e9
    metrics["compiles"] = 0.0
    metrics["correct"] = 0.0
    artifacts = {"failure_reason": reason}
    if log:
        artifacts["log"] = log[-4000:]
    return EvaluationResult(metrics=metrics, artifacts=artifacts)


def evaluate(program_path: str) -> EvaluationResult:
    workdir = tempfile.mkdtemp(prefix="smooth_evolve_")
    try:
        shutil.copyfile(program_path, os.path.join(workdir, "evolved_plan.hpp"))
        binary_path = os.path.join(workdir, "harness")

        compile_cmd = [
            CXX,
            "-std=c++17",
            "-O2",
            "-Wall",
            "-I",
            str(INCLUDE_DIR),
            "-I",
            workdir,
            str(HARNESS_CPP),
            "-o",
            binary_path,
        ]
        try:
            compile_proc = subprocess.run(
                compile_cmd, capture_output=True, text=True, timeout=COMPILE_TIMEOUT_S
            )
        except subprocess.TimeoutExpired:
            return _failure("Compilation timed out")

        if compile_proc.returncode != 0:
            return _failure("Compilation failed", compile_proc.stderr)

        try:
            run_proc = subprocess.run(
                [binary_path], capture_output=True, text=True, timeout=RUN_TIMEOUT_S
            )
        except subprocess.TimeoutExpired:
            return _failure("Harness timed out (infinite loop in a reduction?)")

        if run_proc.returncode != 0:
            return _failure(
                f"Harness crashed (exit {run_proc.returncode})",
                run_proc.stdout + "\n" + run_proc.stderr,
            )

        try:
            last_line = [line for line in run_proc.stdout.splitlines() if line.strip()][-1]
            data = json.loads(last_line)
        except Exception as e:
            return _failure(f"Couldn't parse harness output ({e})", run_proc.stdout + run_proc.stderr)

        if not data.get("ok"):
            return _failure("Harness reported an exception", data.get("error", "unknown"))

        if not data.get("correct"):
            mismatches = ", ".join(data.get("mismatches", []))
            return _failure(f"Wrong result vs. DefaultPlan on: {mismatches}")

        counters = data["counters"]
        metrics = {name: float(counters.get(name, 0)) for name in TRACKED_METRICS}
        # Single scalar fitness for elitism/selection: total operation cost,
        # negated so higher (closer to 0) is better. The five tracked
        # metrics are also registered as MAP-Elites feature dimensions (see
        # config.yaml), which is what actually drives exploring the
        # tradeoffs between them rather than collapsing to one "best" plan.
        metrics["combined_score"] = -sum(metrics.values())
        metrics["compiles"] = 1.0
        metrics["correct"] = 1.0

        artifacts = {}
        if compile_proc.stderr.strip():
            artifacts["compiler_warnings"] = compile_proc.stderr[-4000:]

        return EvaluationResult(metrics=metrics, artifacts=artifacts)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)
