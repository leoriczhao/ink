---
name: ink-dev
description: Use when contributing code to ink and needing repository-specific branching, local validation, PR, and CI conventions.
---

# ink-dev

## Overview

Use this skill to follow the standard contributor workflow for the ink 2D rendering library.

**Related:** Use `ink-release` for nightly/stable release policy, gates, and hotfix handling.

This repository uses a PR-first flow even for solo development: work on branch, open PR, merge to `master`.

## When to Use

Use when:
- starting a feature, bugfix, or refactor in this repo
- preparing a pull request to `master`
- checking what local commands to run before push
- preparing code for release readiness checks

Do not use when you need language-level coding patterns unrelated to repository workflow.

## Branch Strategy

- Treat `master` as protected; use PRs instead of direct pushes.
- Feature branch: `feat/<short-description>`
- Bugfix branch: `fix/<short-description>`
- Refactor branch: `refactor/<short-description>`
- Do not commit feature work directly on `master`.

## Development Cycle

1. Create branch from latest `master`:
   ```bash
   git switch master
   git pull
   git switch -c feat/<name>
   ```
2. Commit with conventional prefixes:
   - `<type>: <concise description>`
   - Types: `feat`, `fix`, `refactor`, `test`, `docs`, `chore`
3. Update `README.md` when API/behavior/build/release workflow changed.
4. Run local validation before push:
   ```bash
   cmake -B build -DINK_BUILD_TESTS=ON -DINK_ENABLE_GL=OFF
   cmake --build build -j$(nproc)
   ctest --test-dir build --output-on-failure
   ```
5. Push branch and open PR to `master`:
   ```bash
   git push -u origin feat/<name>
   gh pr create --base master --title "<type>: <description>" --body "## Summary\n- ..."
   ```

6. Wait for CI checks before merge.
7. Merge via GitHub (squash merge is the default convention).

## Quick Reference

| Area | Current convention |
|------|--------------------|
| Target branch | `master` |
| CI trigger | PR to `master`, push to `master` |
| CI checks | `Format`, `Build & Test` (gcc/clang x Debug/Release), `clang-tidy`, `Sanitizers` |
| Nightly trigger | schedule/manual in `nightly.yml` on default branch |
| Stable release trigger | push tag `v*` |
| Release outputs | CPU tarball, GL tarball, source tarball, GitHub Release notes |

## Code Conventions

- C++17 with strict warnings (`-Wall -Wextra -Wpedantic`).
- Public headers live in `include/ink/`; implementation in `src/`.
- GPU backend sources live in `src/gpu/<api>/`.
- New features include tests in `tests/` (typically `test_<component>.cpp`).
- Tests use Google Test (`TEST`, `EXPECT_EQ`, and related assertions).

## Common Mistakes

- Skipping local `ctest` and relying only on CI feedback.
- Forgetting README updates when public behavior or usage changes.
- Using vague commit titles instead of typed prefixes.
- Pushing release tags before the version bump PR is merged.
