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
4. Run the complete local matrix:

   ```bash
   cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
   cmake --build build-release --parallel
   ctest --test-dir build-release --output-on-failure
   bash scripts/test_install.sh
   prek run --all-files
   doxygen Doxyfile
   ```

5. Push the release commit and wait for every required GitHub Actions job on
   `main` to pass.

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
payload is final, checksum all four payloads:

```bash
shasum -a 256 soft-fp-${version}.tar.gz soft-fp-${version}.zip \
  soft-fp-${version}-docs.zip soft-fp-${version}.spdx.json > SHA256SUMS
```

## Publish and verify

Create a non-draft, non-prerelease GitHub release named `soft-fp X.Y.Z` using
the matching changelog section. Upload both source archives, `SHA256SUMS`, the
documentation archive, and the SPDX SBOM.

Verify after publication:

- the release is marked latest and resolves to the annotated tag;
- every uploaded asset downloads and matches `SHA256SUMS`;
- the repository description, topics, README version, CMake package version,
  Doxygen version, changelog, and security support policy agree;
- `main`, `origin/main`, and the release commit are identical and the local
  worktree is clean.
