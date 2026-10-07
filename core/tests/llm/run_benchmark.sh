#!/usr/bin/env bash
# ============================================================
#
# This file is a part of digiKam project
# https://www.digikam.org
#
# SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# ============================================================

set -euo pipefail

cd "$(dirname "$0")"

QUERIES="queries.json"
OUTDIR="results"
mkdir -p "$OUTDIR"

# Default model location used by digiKam; override via MODEL_DIR.
DIGIKAM_MODEL="${HOME}/.local/share/digikam/facesengine/qwen2.5-1.5b-instruct-q4_k_m.gguf"
MODEL_DIR="${MODEL_DIR:-models}"

run() {
    local name="$1" path="$2"
    if [[ -f "$path" ]]; then
        echo "=== Benchmarking ${name} ==="
        python3 benchmark.py --model "$path" --queries "$QUERIES" --out "${OUTDIR}/${name}.json"
        echo
    else
        echo "--- Skipping ${name}: model not found at ${path} ---"
        echo
    fi
}

# Qwen2.5-1.5B: prefer the model digiKam already installs, else models/.
if [[ -f "$DIGIKAM_MODEL" ]]; then
    run "qwen"       "$DIGIKAM_MODEL"
else
    run "qwen"       "${MODEL_DIR}/qwen2.5-1.5b-instruct-q4_k_m.gguf"
fi

run "tinyllama"      "${MODEL_DIR}/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"
run "qwen3b"         "${MODEL_DIR}/qwen2.5-3b-instruct-q4_k_m.gguf"

echo "Done. Results in ${OUTDIR}/."