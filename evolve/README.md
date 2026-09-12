# Evolving Plan strategies with openevolve

Uses [openevolve](https://github.com/codelion/openevolve) (an open
implementation of AlphaEvolve) to evolve `smooth::EvolvedPlan` -- a `Plan`
strategy (see `include/smooth/plan.hpp`) -- against the same profile battery
`src/profile_runner.cpp` measures (`include/smooth/profile_zoo.hpp`), and
extracts the Pareto frontier across five cost metrics: `bit_operations`,
`scalar_operations`, `carries`, `total_converts`, `atomic_transforms`.

## Setup

openevolve requires Python >= 3.10; the repo's own tooling doesn't assume
that, so it lives in its own venv under `evolve/.venv`, not the system
Python.

```sh
python3.11 -m venv evolve/.venv     # any Python >= 3.10 works
evolve/.venv/bin/pip install -r evolve/requirements.txt
cp evolve/secrets.yaml.example evolve/secrets.yaml
# edit evolve/secrets.yaml: fill in api_key (a Claude API key by default --
# config.yaml points at Anthropic's OpenAI-compatible endpoint; see the
# comments in secrets.yaml.example for pointing at a different provider)
```

By default this runs Claude models (`claude-sonnet-5` / `claude-haiku-4-5-20251001`
in `config.yaml`) through Anthropic's OpenAI-compatible endpoint -- openevolve
only ever speaks the OpenAI chat-completions protocol to a model, and
Anthropic's API happens to answer to that protocol too (see
`config.yaml`'s `llm:` comment). No `provider:` setting needed; that's just
`api_base` + Claude model names. If you'd rather skip the API key entirely
and use this Claude Code CLI session's own login instead, set
`provider: "claude_code"` under `llm:` in `config.yaml` with model names
like `"sonnet"`/`"opus"` -- see openevolve's `llm/claude_code.py` -- though
note that shells out to `claude -p` once per LLM call (every iteration,
times however many models are in the ensemble), so cost tracks your Claude
Code usage instead of a separate API bill.

## Running

```sh
evolve/.venv/bin/python evolve/run.py                    # full run (config.yaml's max_iterations)
evolve/.venv/bin/python evolve/run.py --iterations 20     # quick smoke test
evolve/.venv/bin/python evolve/run.py --output evolve/output2
evolve/.venv/bin/python evolve/run.py --checkpoint evolve/output/checkpoints/checkpoint_50
```

Each iteration: the LLM edits `EvolvedPlan`, `evaluator.py` compiles
`harness.cpp` against it, runs it, checks every profile's result against
`DefaultPlan` (ground truth), and sums its Metrics counters across all
profiles. That gives openevolve both a single scalar fitness
(`combined_score = -sum(the five metrics)`) and, more importantly, the five
raw metrics as MAP-Elites feature dimensions (see `config.yaml`) -- that's
what keeps a spread of different tradeoffs alive instead of collapsing to
one "best" plan.

When it finishes, `run.py` calls `pareto.py` to compute the actual
Pareto-optimal set (not just what MAP-Elites happened to keep) from every
program openevolve ever checkpointed, and writes:

- `evolve/output/pareto_frontier.md` -- a table + full source of each
  frontier program.
- `evolve/output/pareto_frontier.json` -- the same, machine-readable.
- `evolve/output/pareto_programs/<id>.hpp` -- each frontier program's code,
  ready to drop into `include/smooth/plan_zoo/` if you want to keep one.

Run `evolve/.venv/bin/python evolve/pareto.py evolve/output` any time to
recompute the report without a fresh evolution run (e.g. after a manual
`--checkpoint` resume).

## Files

- `initial_program.hpp` -- the seed `EvolvedPlan`, with the mutable region
  marked by `# EVOLVE-BLOCK-START`/`END` comments (openevolve's own
  convention regardless of language -- the literal substring `# EVOLVE-...`
  has to appear in the line, hence the `#` inside a `//` comment).
- `harness.cpp` -- compiled fresh per candidate; runs every profile against
  both `DefaultPlan` and `EvolvedPlan`, checks they agree, sums
  `EvolvedPlan`'s Metrics, prints one JSON line.
- `evaluator.py` -- openevolve's `evaluate(program_path)` entry point:
  drives the compile + run above and turns the result into an
  `EvaluationResult`. A candidate that fails to compile, crashes, times
  out, or disagrees with `DefaultPlan` gets a large penalty on every
  tracked metric plus a `failure_reason`/`log` artifact, which openevolve
  feeds back into the next prompt.
- `config.yaml` -- population/island/feature-dimension settings, and the
  domain-briefing system prompt (Plan's API, what each metric measures,
  which existing `plan_zoo/` strategies do well and why, the known
  `RowSpreadTransformation`/`SpreadTransformation` atomize() landmine).
- `secrets.yaml` (gitignored, not checked in) -- just the API key (a Claude
  API key by default; `run.py` merges it into the config loaded from
  `config.yaml`).
- `pareto.py` -- the Pareto-front extraction, usable standalone.
- `run.py` -- ties it all together.

## Why a separate venv instead of touching the C++ build

Nothing here changes `include/smooth/` or `src/profile_runner.cpp` at
evolve-time -- `evaluator.py` compiles `harness.cpp` directly against the
repo's `include/` with a plain `c++` invocation (see `evaluator.py`'s
`compile_cmd`), into a throwaway temp directory per candidate. It never
touches `build/` or CMake. If a frontier program looks worth keeping
permanently, copy it from `evolve/output/pareto_programs/` into
`include/smooth/plan_zoo/` by hand and wire it into
`src/profile_runner.cpp`'s `planKinds()`, the same way the existing
strategies there are.
