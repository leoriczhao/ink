---
name: ink-release
description: Use when preparing nightly or stable releases in ink and needing tag rules, version checks, release gates, and hotfix handling.
disable-model-invocation: true
allowed-tools: Bash Read Grep Glob
---

# ink-release

## Overview

Use this skill for a single-maintainer release workflow with two channels:
- `nightly`: frequent, auto-generated validation artifacts
- `stable`: semver tags for public consumption

Core principle: ship often on nightly, gate strictly for stable.

Repository policy remains PR-first: all release-related version/changelog edits land via PR before tagging.

## When to Use

Use when:
- defining or running release steps for this repository
- deciding whether a change goes to nightly or stable
- preparing a tag (`vX.Y.Z`) and GitHub Release
- handling a bad release and planning hotfixes

Do not use when you need everyday feature-development workflow; use `ink-dev` for that.

## Release Model

### Nightly channel

- Source: `master` HEAD
- Trigger: scheduled run (recommended daily) or manual dispatch
- Naming: `nightly-YYYYMMDD-<shortsha>`
- Promise: validation and early integration only; no compatibility guarantees
- Note: nightly can run only after `.github/workflows/nightly.yml` exists on the default branch.

### Stable channel

- Source: release tag `vX.Y.Z`
- Trigger: manual tag push
- Promise: compatibility and changelog-backed release notes

### Version policy

- `major`: breaking API/behavior changes
- `minor`: backward-compatible features
- `patch`: fixes/performance/docs without breaking behavior

## Stable Gate Checklist

All items must pass before creating `vX.Y.Z`:

1. `master` CI is green (`ci.yml`).
2. `CHANGELOG.md` includes a complete section for `X.Y.Z`.
3. Version matches tag across **all** files: `CMakeLists.txt`, `include/ink/version.hpp`, `Doxyfile`, `docs/conf.py`.
4. Local smoke check succeeds with release-like config.

One-command preflight (validates all of the above):

```bash
./scripts/release-preflight.sh X.Y.Z
```

Recommended smoke commands (run both variants if environment supports GL):

```bash
# CPU-only
cmake -B build -DCMAKE_BUILD_TYPE=Release -DINK_BUILD_TESTS=ON -DINK_ENABLE_GL=OFF
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure

# With GL (if available)
cmake -B build-gl -DCMAKE_BUILD_TYPE=Release -DINK_BUILD_TESTS=ON -DINK_ENABLE_GL=ON
cmake --build build-gl -j$(nproc)
ctest --test-dir build-gl --output-on-failure
```

## Stable Release Steps

1. Update version using the bump script and update changelog in a normal PR:
   ```bash
   ./scripts/bump-version.sh X.Y.Z
   ```
   This updates CMakeLists.txt, include/ink/version.hpp, Doxyfile, and docs/conf.py in one command.
2. Merge to `master` after CI passes.
3. Create and push tag:

```bash
git tag vX.Y.Z
git push origin vX.Y.Z
```

4. Let `release.yml` build, test, package, and publish.
5. Perform post-release smoke check by downloading an artifact and validating minimal integration.

## Nightly Operations

- Nightly failures do not block all development, but should be fixed before the next stable cut.
- If nightly stays red for 2+ consecutive runs, create a `fix/ci-*` branch and prioritize the fix before new feature work.
- Keep nightly notes short: commit range + notable risks.

Nightly workflow file: `.github/workflows/nightly.yml`.

## Hotfix Policy

- Never delete published stable tags.
- If stable release is bad, publish `vX.Y.(Z+1)` as hotfix.
- If a recently merged PR caused the issue, prefer `git revert` on `master` first to restore stability, then fix forward in a new PR.
- Mark affected release notes as deprecated and link to the fixed version.

## Quick Reference

| Topic | Rule |
|------|------|
| Stable tag format | `vX.Y.Z` |
| Stable source branch | `master` |
| Nightly name | `nightly-YYYYMMDD-<shortsha>` |
| Mandatory gates | CI green + changelog + version match (all files) + smoke |
| Recovery | revert bad merge, then new hotfix release; no tag deletion |

## Common Mistakes

- Tagging before changelog/version updates are merged.
- Only checking `CMakeLists.txt` version — `version.hpp`, `Doxyfile`, and `docs/conf.py` must also match.
- Treating nightly artifacts as stable compatibility promises.
- Releasing while CI is flaky and hoping downstream users will not hit issues.
- Trying to rewrite history instead of shipping a clean hotfix version.
