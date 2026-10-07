# Benchmark results: choosing a local model for NL search

## Question

digiKam's natural-language search runs a **local, quantised LLM** on the
user's own machine to translate a plain-language query into a structured
search intent. Which small model is the right one - good enough to be useful,
small and fast enough to run on ordinary hardware alongside the application?

This benchmark answers that empirically, comparing three candidates on the
same ~40 labelled queries, using the exact prompt the application sends.

## Candidates

Small, locally-runnable, permissively-licensed models were considered. Three
were benchmarked directly:

- **TinyLlama-1.1B** - the lightweight baseline.
- **Qwen2.5-1.5B-Instruct** - the current choice.
- **Qwen2.5-3B-Instruct** - to test whether a larger model does better.

All were run as Q4_K_M GGUFs for a like-for-like comparison, and all received
the identical raw prompt (matching how the C++ backend feeds the model - no
chat-template wrapping).

## Method

- **Dataset:** ~40 hand-labelled queries covering every supported field
  (single and composite), value-case handling, and three safe-failure
  behaviours (ambiguous -> clarification, unsupported field -> unresolved,
  nonsense -> empty).
- **Prompt:** transcribed verbatim from `SearchPromptBuilder`, so the
  benchmark exercises the real pipeline's prompt rather than a reimplementation.
- **Scoring:** at the parsed-intent level (field/op/value, lower-cased),
  which isolates the model's accuracy from the resolver's later,
  model-independent case resolution.
- **Metrics:** constraint accuracy, per-query latency (median / p90 / max),
  and peak resident memory.

This is a task-specific comparison for digiKam's schema, not a general LLM
benchmark. Latency is CPU-bound and varies run-to-run; the first query of a
run also pays a one-time model warm-up cost, so **median** latency is the
representative figure.

## Results

| Model | Constraint accuracy | Median latency | Peak RAM |
|-------|--------------------:|---------------:|---------:|
| TinyLlama-1.1B  | 18% (6/34)  |  ~6.4s | ~1.3 GB |
| **Qwen2.5-1.5B** | **85% (29/34)** | **~2.3s** | **~2.0 GB** |
| Qwen2.5-3B      | 79% (27/34) | ~29s  | ~3.5 GB |

### TinyLlama-1.1B - not viable

At 18% it fails on nearly every field. Its errors are not subtle: malformed
JSON, invented field names, typos in values, and in several cases copying the
schema template literally into its output instead of filling it in. It was
also *slower* than the 1.5B model, not faster, because it tends to ramble past
a valid answer. Too inaccurate to use.

### Qwen2.5-1.5B - the choice

At 85% it reliably maps queries to the correct field/op/value. Its errors
cluster tightly, on **orientation** ("portrait" read as a tag; "horizontally"
not mapped to landscape) and **date-range structure** (occasionally the wrong
operator). These are precisely the cases the prompt rules and the capability
dictionary already handle in the running system. Median latency ~2.3s and
~2 GB RAM are acceptable for an interactive feature.

### Qwen2.5-3B - bigger is not better

The larger model scored **lower** on core constraints (79% vs 85%) - it tends
to over-elaborate on simple structured tasks, and newly mis-handled some cases
the 1.5B model got right (e.g. person queries). Critically, its median latency
was ~29s (first query 73s), and it used ~3.5 GB. A ~29s response is unusable
for an interactive search box. It did handle a couple of ambiguous queries
better on its own - but those are exactly the cases the dictionary already
covers downstream.

## Safe-failure behaviour

On its own, no model reliably handled the ambiguous / unsupported / nonsense
queries - the raw model guesses (e.g. inventing a constraint for "videos
longer than 5 minutes", a field the schema does not support). This is expected
and safe: in the real system the **parser whitelist** rejects any field
outside the supported set, and the **capability dictionary** detects
ambiguity. The benchmark makes visible that this safety comes from those
layers, not from the model.

## Conclusions

1. **1.5B is the sweet spot.** The comparison bounds the choice from both
   sides: the smaller model is far too inaccurate, and the larger model is
   slower, heavier, and no more accurate on the core task. Qwen2.5-1.5B gives
   the best accuracy *and* the best latency.

2. **Fine-tuning (LoRA) is not warranted.** The residual 1.5B errors
   (orientation, dates) are **not** a model-capacity problem: a 2x-larger
   model has the same errors. They are prompt/vocabulary issues, and they are
   already mitigated by the prompt rules and the capability dictionary. Since
   neither a larger model nor the existing prompt layer leaves a recurring,
   prompt-resistant error class, a LoRA adapter would add training and
   packaging complexity for no measurable benefit. The plan's conditional -
   "if prompting is sufficient, document that and use the time for polish" -
   applies.

3. **The architecture is validated.** The model's weak spots are exactly the
   ones the surrounding pipeline was built to handle, and its unsafe raw
   behaviour on out-of-scope input is exactly what the whitelist and
   dictionary exist to catch.