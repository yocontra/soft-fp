# Releasing soft-fp

Releases are cut from `main` after both the branch and tag CI matrices pass.
Published artifacts are generated from the annotated tag, never from an
uncommitted worktree.

## Prepare

1. Set the version in `CMakeLists.txt` and `Doxyfile`.
2. Move the accumulated changelog entries from `Unreleased` into a dated
   version section and update its comparison links.
3. Update the stable-version statement in `README.md` and the supported lines
   in `SECURITY.md` when appropriate.
4. Run the local Release preflight:

   ```bash
   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
   cmake --build build-release --parallel
   ctest --test-dir build-release --output-on-failure
   bash scripts/test_install.sh
   prek run --all-files
   doxygen Doxyfile
   ```

5. Run the [benchmark procedure](bench/README.md#ci-usage): collect exactly
   three complete 500 ms samples, aggregate their per-operation minima, and
   compare against the baseline with the documented budgets. Retain all raw
   samples and failures. This estimates best-case throughput, not tail latency;
   branch and tag CI do not run the PR-only benchmark job.
6. Push the release commit and wait for every required job in the
   [CI workflow](.github/workflows/ci.yml) on `main` to pass. That matrix also
   covers Debug builds, alternate exception modes, OpenCL/FTZ, sanitizers,
   and supported platforms; the local preflight above does not replace it.

## Tag

Create an annotated tag at the exact green commit:

```bash
git tag -a vX.Y.Z -m "soft-fp vX.Y.Z"
git push origin vX.Y.Z
```

Tag pushes run the CI matrix again. Do not publish the GitHub release until
that run passes.

## Build release artifacts

Use a temporary directory outside the checkout. `gzip -n` removes the gzip
header timestamp; `git archive` takes file modes, contents, and timestamps
from the tagged tree.

```bash
version=X.Y.Z
tag=v${version}
prefix=soft-fp-${version}/

git archive --format=tar --prefix="${prefix}" "${tag}" \
  | gzip -n -9 > "soft-fp-${version}.tar.gz"
git archive --format=zip --prefix="${prefix}" "${tag}" \
  > "soft-fp-${version}.zip"
```

Extract each archive into a fresh directory and repeat the Release build,
tests, and install-consumer smoke test before upload. Generate Doxygen HTML
from the extracted source and package it as `soft-fp-X.Y.Z-docs.zip`. Export
GitHub's dependency-graph SBOM as `soft-fp-X.Y.Z.spdx.json`. After every
payload is final, checksum the required payloads:

```bash
shasum -a 256 soft-fp-${version}.tar.gz soft-fp-${version}.zip \
  soft-fp-${version}-docs.zip soft-fp-${version}.spdx.json > SHA256SUMS
```

When refreshing a benchmark baseline, also package the raw calibration samples,
source/build/environment metadata, and every candidate comparison (including
failed diagnostics) as `soft-fp-X.Y.Z-benchmark-calibration.zip`. Include that
optional payload in the checksum manifest before upload:

```bash
shasum -a 256 soft-fp-${version}-benchmark-calibration.zip >> SHA256SUMS
```

## Publish and verify

Create a non-draft, non-prerelease GitHub release named `soft-fp X.Y.Z` using
the matching changelog section. Upload both source archives, `SHA256SUMS`, the
documentation archive, the SPDX SBOM, and any benchmark calibration archive.

Verify after publication:

- the release is marked latest and resolves to the annotated tag;
- every uploaded asset downloads and matches `SHA256SUMS`;
- the repository description, topics, README version, CMake package version,
  Doxygen version, changelog, and security support policy agree;
- `main`, `origin/main`, and the release commit are identical and the local
  worktree is clean.
