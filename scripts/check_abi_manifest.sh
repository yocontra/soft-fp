#!/usr/bin/env bash
# Verify that every function named by enabled public headers has a definition
# in the configured archives. This catches declarations drifting away from
# feature selection or implementation source manifests.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:?usage: check_abi_manifest.sh BUILD_DIR}
config="$build/generated/soft_fp64/config.h"
test -f "$config"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

perl -0777 -ne \
  '$t=$_; $t=~s!/\*.*?\*/!!gs; $t=~s!//[^\n]*!!g; while($t=~/\b(sf(?:64|128|256)_[A-Za-z0-9_]+)\s*\(/g){print "$1\n"}' \
  "$root/include/soft_fp64/soft_f64.h" \
  "$root/include/soft_fp128/soft_f128.h" \
  "$root/include/soft_fp256/soft_f256.h" | sort -u > "$tmp/declared-all"

cp "$tmp/declared-all" "$tmp/declared"
if grep -q '#define SOFT_FP64_HAS_OCL_ABI 0' "$config"; then
  grep -Ev '^sf64_(ocl|native)_' "$tmp/declared" > "$tmp/filtered"
  mv "$tmp/filtered" "$tmp/declared"
fi
if grep -q '#define SOFT_FP_BUILD_FP128 0' "$config"; then
  grep -Ev '^sf128_' "$tmp/declared" > "$tmp/filtered"
  mv "$tmp/filtered" "$tmp/declared"
fi
if grep -q '#define SOFT_FP_BUILD_FP256 0' "$config"; then
  grep -Ev '^sf256_' "$tmp/declared" > "$tmp/filtered"
  mv "$tmp/filtered" "$tmp/declared"
fi

archives=()
for archive in \
  "$build/libsoft_fp64.a" \
  "$build/src/fp128/libsoft_fp128.a" \
  "$build/src/fp256/libsoft_fp256.a"; do
  if [ -f "$archive" ]; then
    archives+=("$archive")
  fi
done
if [ "${#archives[@]}" -eq 0 ]; then
  echo "check_abi_manifest: no soft-fp archives in $build" >&2
  exit 2
fi

nm -g --defined-only "${archives[@]}" | sed -nE \
  's/^.*[[:space:]]_?(sf(64|128|256)_[A-Za-z0-9_]+)$/\1/p' | sort -u > "$tmp/defined"

# Production archives must never leak Berkeley SoftFloat's unprefixed public
# API or state. Internal `softfloat_*` helpers are hidden and backend-local.
nm -g --defined-only "${archives[@]}" | sed -nE \
  's/^.*[[:space:]]_?([A-Za-z][A-Za-z0-9_]*)$/\1/p' | \
  grep -E '^(f128_|f64_to_f128$|[iu](32|64)_to_f128$|softfloat_(detectTininess|exceptionFlags|roundingMode)$|extF80_roundingPrecision$)' \
  > "$tmp/upstream-leaks" || true
if [ -s "$tmp/upstream-leaks" ]; then
  echo "check_abi_manifest: unprefixed SoftFloat symbols leaked:" >&2
  sed 's/^/  /' "$tmp/upstream-leaks" >&2
  exit 1
fi

comm -23 "$tmp/declared" "$tmp/defined" > "$tmp/missing"
if [ -s "$tmp/missing" ]; then
  echo "check_abi_manifest: public declarations without definitions:" >&2
  sed 's/^/  /' "$tmp/missing" >&2
  exit 1
fi
echo "check_abi_manifest: clean ($(wc -l < "$tmp/declared" | tr -d ' ') symbols)"
