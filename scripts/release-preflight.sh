#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'Usage: %s <version>\n' "$0"
  printf 'Example: %s 0.2.1\n' "$0"
  exit 1
fi

VERSION="$1"
TAG="v${VERSION}"

printf '== release preflight for %s ==\n' "$TAG"

if [[ -n "$(git status --porcelain)" ]]; then
  printf 'FAIL: working tree is dirty. Commit or stash changes first.\n'
  exit 1
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ "$CURRENT_BRANCH" != "master" ]]; then
  printf 'FAIL: current branch is %s (expected master).\n' "$CURRENT_BRANCH"
  exit 1
fi

if ! git rev-parse --abbrev-ref --symbolic-full-name @{upstream} >/dev/null 2>&1; then
  printf 'FAIL: no upstream configured for master.\n'
  exit 1
fi

read -r BEHIND AHEAD < <(git rev-list --left-right --count @{upstream}...HEAD)

if [[ "${BEHIND}" != "0" ]]; then
  printf 'FAIL: local master is behind upstream. Run git pull first.\n'
  exit 1
fi

if [[ "${AHEAD}" != "0" ]]; then
  printf 'FAIL: local master has unpushed commits. Push or reconcile first.\n'
  exit 1
fi

if git rev-parse --verify --quiet "refs/tags/${TAG}" >/dev/null; then
  printf 'FAIL: tag %s already exists locally.\n' "$TAG"
  exit 1
fi

if git ls-remote --exit-code --tags origin "refs/tags/${TAG}" >/dev/null 2>&1; then
  printf 'FAIL: tag %s already exists on origin.\n' "$TAG"
  exit 1
fi

if ! grep -q "project(ink VERSION ${VERSION}" CMakeLists.txt; then
  printf 'FAIL: CMakeLists.txt version does not match %s.\n' "$VERSION"
  exit 1
fi

if ! grep -q "## \[${VERSION}\]" CHANGELOG.md; then
  printf 'FAIL: CHANGELOG.md missing section for [%s].\n' "$VERSION"
  exit 1
fi

printf 'Running smoke build/test...\n'
cmake -B build -DCMAKE_BUILD_TYPE=Release -DINK_BUILD_TESTS=ON -DINK_ENABLE_GL=OFF
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

printf 'PASS: preflight checks succeeded for %s\n' "$TAG"
