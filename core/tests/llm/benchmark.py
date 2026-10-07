#!/usr/bin/env python3
# ============================================================
#
# This file is a part of digiKam project
# https://www.digikam.org
#
# SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# ============================================================

"""NL-search model benchmark: latency, memory, structured-output accuracy.

Mirrors SearchPromptBuilder::buildPrompt exactly (raw prompt, no chat template,
matching SearchLlamaWorker::slotDoInference which tokenizes the raw string).
"""

import argparse, json, time, os
from datetime import date
from llama_cpp import Llama
import psutil

# --- Prompt: transcribed from searchpromptbuilder.cpp ---

SYSTEM_PROMPT = (
    "You translate photo search requests into JSON.\n"
    "Respond with ONLY a single JSON object. No prose, no markdown.\n"
    "If a term is subjective or cannot be mapped to a supported field, "
    "do NOT guess: either omit it or use the clarification object.\n"
    "Never invent tags, places, or dates that are not implied by the request.\n"
    "Never map a word to a field it does not belong to: a word like "
    "\"vacation\" is not a person and \"nice\" is not a caption. "
    "Returning fewer constraints is always better than returning wrong ones. "
    "If nothing maps cleanly, return {\"constraints\":[]} with a "
    "clarification asking the user to refine.\n"
)

SCHEMA_DESCRIPTION = (
    "JSON schema:\n"
    "{\n"
    "  \"constraints\": [ { \"field\": F, \"op\": O, \"value\": V } ],\n"
    "  \"clarification\": null | { \"message\": M, \"choices\": [C1, C2] }\n"
    "}\n"
    "Supported fields F: tag, album, person, place, daterange, rating, "
    "picklabel, colorlabel, orientation, caption\n"
    "Supported ops O: eq, contains, gte, lte, between\n"
    "Date values MUST be a range in the form YYYY-MM-DD..YYYY-MM-DD, "
    "with op \"between\". A whole year like 2023 becomes "
    "2023-01-01..2023-12-31. A whole month like March 2024 becomes "
    "2024-03-01..2024-03-31. Never output a single bare date.\n"
    "Words like flagged, picked, accepted, rejected or pending mean "
    "picklabel, not tag. Values for picklabel are: accepted, rejected, "
    "pending, none.\n"
    "Colour words like red, green or blue followed by label mean "
    "colorlabel, not tag.\n"
    "City, country, or location names (Paris, France, New York, Tokyo) "
    "mean the place field. Never map a location name to a colour or tag.\n"
    "Use op \"eq\" for an exact rating, and \"gte\" when the request says "
    "at least, minimum, or or more.\n"
    "Example: request \"photos from Paris\" produces:\n"
    "{ \"constraints\": [ "
    "{ \"field\": \"place\", \"op\": \"eq\", \"value\": \"Paris\" } ] }\n"
    "Example: request \"photos from 2023 rated 5 stars\" produces:\n"
    "{ \"constraints\": [ "
    "{ \"field\": \"daterange\", \"op\": \"between\", "
    "\"value\": \"2023-01-01..2023-12-31\" }, "
    "{ \"field\": \"rating\", \"op\": \"eq\", \"value\": 5 } ] }\n"
    "Example: request \"flagged photos rated at least 3 stars\" produces:\n"
    "{ \"constraints\": [ "
    "{ \"field\": \"picklabel\", \"op\": \"eq\", \"value\": \"accepted\" }, "
    "{ \"field\": \"rating\", \"op\": \"gte\", \"value\": 3 } ] }\n"
)

def build_prompt(query):
    today = date.today()
    p = SYSTEM_PROMPT + "\n" + SCHEMA_DESCRIPTION
    p += (f"\nToday's date is {today.isoformat()}. Resolve relative dates "
          f"against it: \"last year\" means {today.year-1}-01-01..{today.year-1}-12-31, "
          f"\"this year\" means {today.year}-01-01..{today.year}-12-31.\n")
    p += "\nUser request: " + query + "\nJSON:"
    return p


def extract_json(text):
    start = text.find('{')
    if start < 0:
        return None
    depth = 0
    for i in range(start, len(text)):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                try:
                    return json.loads(text[start:i+1])
                except Exception:
                    return None
    return None

def constraint_set(constraints):
    s = set()
    for c in constraints or []:
        s.add((str(c.get("field", "")).lower(),
               str(c.get("op", "")).lower(),
               str(c.get("value", "")).lower()))
    return s

def score(entry, out):
    if entry.get("expected_clarification"):
        return {"type": "clarification",
                "correct": bool(out and out.get("clarification"))}
    if entry.get("expected_empty"):
        cons = out.get("constraints", []) if out else []
        return {"type": "empty", "correct": len(cons) == 0}
    if entry.get("expected_unresolved"):
        cons = out.get("constraints", []) if out else []
        asked = bool(out and out.get("clarification"))
        return {"type": "unresolved", "correct": len(cons) == 0 or asked}
    exp = constraint_set(entry["expected"])
    got = constraint_set(out.get("constraints") if out else [])
    tp = len(exp & got)
    return {"type": "constraints", "correct": exp == got,
            "precision": round(tp/len(got), 3) if got else 0.0,
            "recall": round(tp/len(exp), 3) if exp else 0.0,
            "expected": sorted(exp), "got": sorted(got)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--queries", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--n-threads", type=int, default=os.cpu_count())
    args = ap.parse_args()

    with open(args.queries) as f:
        data = json.load(f)["queries"]

    proc = psutil.Process()
    mem_before = proc.memory_info().rss

    llm = Llama(model_path=args.model, n_ctx=2048,
                n_threads=args.n_threads, verbose=False)

    mem_after_load = proc.memory_info().rss
    peak_rss = mem_after_load
    results = []

    for q in data:
        prompt = build_prompt(q["query"])
        t0 = time.perf_counter()
        out = llm(prompt, max_tokens=256, temperature=0.0, echo=False,
                  stop=["\n\n", "\nUser request:", "```", "To clarify", "To resolve"])
        dt = time.perf_counter() - t0
        text = out["choices"][0]["text"]
        parsed = extract_json(text)
        sc = score(q, parsed)
        peak_rss = max(peak_rss, proc.memory_info().rss)
        results.append({"id": q.get("id"), "query": q["query"],
                        "latency_s": round(dt, 3),
                        "raw_output": text.strip(), "parsed": parsed,
                        "score": sc})
        print(f"[{'OK' if sc.get('correct') else 'XX'}] "
              f"{str(q.get('id','?')):22s} {dt:5.2f}s  {q['query']}")


    # Classify results by what they test
    constraint_results = [r for r in results if r["score"]["type"] == "constraints"]
    behavior_results   = [r for r in results if r["score"]["type"] in ("clarification", "unresolved", "empty")]

    constraint_correct = sum(1 for r in constraint_results if r["score"]["correct"])
    behavior_correct   = sum(1 for r in behavior_results if r["score"]["correct"])

    from collections import Counter
    field_errors = Counter()
    for r in constraint_results:
        if not r["score"]["correct"]:
            exp = set(f for f, _, _ in r["score"]["expected"])
            got = set(f for f, _, _ in r["score"]["got"])
            for f in exp ^ got:
                field_errors[f] += 1


    lat = sorted(r["latency_s"] for r in results)
    summary = {
        "model": os.path.basename(args.model),
        "total": len(results),
        "constraint_accuracy": round(constraint_correct / len(constraint_results), 3) if constraint_results else None,
        "constraint_correct": constraint_correct,
        "constraint_total": len(constraint_results),
        "field_error_counts": dict(field_errors),
        "behavior_handled_by_model": behavior_correct,
        "behavior_total": len(behavior_results),
        "latency_median_s": lat[len(lat)//2],
        "latency_p90_s": lat[int(len(lat)*0.9)],
        "latency_max_s": lat[-1],
        "model_load_rss_mb": round((mem_after_load - mem_before)/1e6, 1),
        "peak_rss_mb": round(peak_rss/1e6, 1),
    }
    print("\n== Summary ==")
    for k, v in summary.items():
        print(f"  {k}: {v}")

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "w") as f:
        json.dump({"summary": summary, "results": results}, f, indent=2)
    print(f"\nWrote {args.out}")

if __name__ == "__main__":
    main()