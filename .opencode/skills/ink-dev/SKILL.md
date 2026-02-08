---
name: ink-dev
description: ink project development workflow - branching, PR, CI/CD, and release conventions
---

## ink Development Workflow

This skill defines the development process for the ink 2D rendering library.

### Branch Strategy

- `master` is the protected main branch. Never push directly.
- Feature branches: `feat/<short-description>` (e.g. `feat/add-path-ops`)
- Bug fix branches: `fix/<short-description>` (e.g. `fix/clip-rect-overflow`)
- Refactor branches: `refactor/<short-description>`

### Development Cycle

1. **Create branch** from latest master:
   ```
   git checkout master && git pull
   git checkout -b feat/<name>
   ```

2. **Develop** — make changes, commit often with clear messages:
   ```
   <type>: <concise description>
   ```
   Types: `feat`, `fix`, `refactor`, `test`, `docs`, `chore`

3. **Build and test locally** before pushing:
   ```
   cmake -B build -DINK_BUILD_TESTS=ON -DINK_ENABLE_GL=OFF
   cmake --build build -j$(nproc)
   ctest --test-dir build --output-on-failure
   ```

4. **Push and create PR**:
   ```
   git push -u origin feat/<name>
   gh pr create --base master --title "<type>: <description>" --body "## Summary\n- ..."
   ```

5. **CI must pass** — the `ci` workflow runs lint + build + test on every PR.
   PR cannot merge until the `ci` check passes (branch protection).

6. **Merge** — squash merge into master via GitHub.

### Release Process

1. Update version in `CMakeLists.txt` (`project(ink VERSION x.y.z)`)
2. Merge the version bump PR
3. Tag and push:
   ```
   git tag v0.x.x
   git push origin v0.x.x
   ```
4. The `release` workflow automatically:
   - Builds Release mode
   - Runs tests
   - Packages `libink.a` + headers
   - Creates a GitHub Release with auto-generated notes

### CI/CD Workflows

| Workflow | Trigger | What it does |
|----------|---------|-------------|
| `ci.yml` | PR to master, push to master | lint (clang-format) + build + test |
| `release.yml` | push tag `v*` | build Release + test + package + GitHub Release |

### Code Conventions

- C++17, compile with `-Wall -Wextra -Wpedantic`
- Headers in `include/ink/`, sources in `src/`
- GPU backend sources in `src/gpu/<api>/`
- All new features must have tests in `tests/`
- Test file naming: `test_<component>.cpp`
- Use Google Test (`TEST`, `EXPECT_EQ`, etc.)

### When to use this skill

Use this skill when:
- Starting work on a new feature or bug fix
- Creating a PR
- Preparing a release
- Unsure about the project's branching or CI conventions
