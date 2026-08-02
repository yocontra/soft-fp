#!/usr/bin/env bash
# Fetch comparison libraries for the comparative bench.
# Idempotent: skips clones that already exist.
#
# Licenses:
#   Berkeley SoftFloat 3e    BSD-3
#   ckormanyos/soft_double   Boost-1.0
#
# Both are compatible with our MIT root.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
EXT="$HERE/external"
mkdir -p "$EXT"

clone() {
    local url=$1 dest=$2 ref=$3
    if [ -d "$dest/.git" ]; then
        echo "bench/external: $dest already present — skipping clone"
    else
        echo "bench/external: cloning $url -> $dest"
        git clone --depth 1 "$url" "$dest"
    fi
    ( cd "$dest" && git fetch --depth 1 origin "$ref" >/dev/null 2>&1; git checkout --detach "$ref" )
}

# Immutable audited revisions. Update deliberately alongside benchmark
# compatibility code; never benchmark an unreviewed moving branch head.
clone https://github.com/ucb-bar/berkeley-softfloat-3 "$EXT/softfloat" \
    a0c6494cdc11865811dec815d5c0049fba9d82a8

# ckormanyos/soft_double
clone https://github.com/ckormanyos/soft_double "$EXT/soft_double" \
    cd1abb3880a3a6e3e2ab17c306966a632323a7a8

echo
echo "Done. Now:"
echo "  cmake -S . -B build -DSOFT_FP64_BUILD_BENCH=ON"
echo "  cmake --build build --target bench_compare"
