# NL-search model benchmark

A standalone harness that measures how well small local LLMs translate
natural-language photo-search queries into digiKam's structured search
intent. It reports three things per model: **structured-output accuracy**,
**latency**, and **memory**.

This benchmark is a development tool. It is **not** part of the digiKam
build or CI; it lives under `project/` with the other scripts and data.

## What it measures

For each query in `queries.json`, the harness sends the *same prompt the
application uses* (transcribed from `SearchPromptBuilder`) to a model, parses
the JSON the model returns, and compares the resulting constraints against a
hand-labelled expected intent.

Scoring is done at the **parsed-intent level** (field / op / value, lower-cased),
because that isolates the model's behaviour from the resolver's later,
model-independent steps (such as case resolution). Results are split into:

- **constraint accuracy** - the model's core job: producing the right
  field/op/value triples.
- **behaviour handling** - whether the raw model, on its own, copes with
  ambiguous, unsupported, or nonsensical queries. (In the real system these
  are handled downstream by the capability dictionary and the parser
  whitelist, not by the model - so a low score here is expected and safe.)

## Prerequisites

```bash
pip install llama-cpp-python psutil huggingface_hub --break-system-packages
```

## Getting the models

The GGUF model files are **not** committed (they are large). Download them
into `models/`:

```bash
mkdir -p models
python3 -c "from huggingface_hub import hf_hub_download; hf_hub_download('Qwen/Qwen2.5-1.5B-Instruct-GGUF','qwen2.5-1.5b-instruct-q4_k_m.gguf',local_dir='models')"
python3 -c "from huggingface_hub import hf_hub_download; hf_hub_download('TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF','tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf',local_dir='models')"
python3 -c "from huggingface_hub import hf_hub_download; hf_hub_download('Qwen/Qwen2.5-3B-Instruct-GGUF','qwen2.5-3b-instruct-q4_k_m.gguf',local_dir='models')"
```

The 1.5B model is also the one digiKam installs at
`~/.local/share/digikam/facesengine/`; `run_benchmark.sh` uses that copy if
present.

## Running

All models at once:

```bash
./run_benchmark.sh
```

A single model:

```bash
python3 benchmark.py \
    --model models/qwen2.5-1.5b-instruct-q4_k_m.gguf \
    --queries queries.json \
    --out results/qwen.json
```

Each run prints a per-query pass/fail line and a summary, and writes a
detailed JSON report to `results/`.

## Adding test queries

`queries.json` is a list of entries. A constraint query pairs a natural-language
string with its expected intent:

```json
{ "id": "date-year", "query": "photos from 2023",
  "expected": [{"field": "daterange", "op": "between",
                "value": "2023-01-01..2023-12-31"}] }
```

Expected values are lower-cased and use absolute dates (so scoring is
deterministic regardless of when the benchmark runs). Behaviour cases use a
flag instead of `expected`:

- `"expected_clarification": true` - an ambiguous query that should prompt a
  clarification.
- `"expected_unresolved": true` - names a field the schema does not support.
- `"expected_empty": true` - maps to nothing.

## Expected results

See `RESULTS.md` for the full write-up. In summary, on ~40 labelled queries:

| Model | Constraint accuracy | Median latency | Peak RAM |
|-------|--------------------:|---------------:|---------:|
| TinyLlama-1.1B  | ~18% |  ~6s | ~1.3 GB |
| **Qwen2.5-1.5B** (chosen) | **~85%** | **~2s** | **~2.0 GB** |
| Qwen2.5-3B      | ~79% | ~29s | ~3.5 GB |

Numbers vary somewhat run-to-run (CPU inference latency in particular).