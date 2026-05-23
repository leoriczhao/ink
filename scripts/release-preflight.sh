#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'Usage: %s <version>\n' "$0"
  printf 'Example: %s 0.2.1\n' "$0"
  exit 1
fi

VERSION="$1"
TAG="v${VERSION}"

fail() {
  printf 'FAIL: %s\n' "$1"
  exit 1
}

printf '== release preflight for %s ==\n' "$TAG"

if ! [[ "$VERSION" =~ ^[0-9]+[.][0-9]+[.][0-9]+$ ]]; then
  fail "version must be in X.Y.Z format."
fi

if [[ -n "$(git status --porcelain)" ]]; then
  fail "working tree is dirty. Commit or stash changes first."
fi

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [[ "$CURRENT_BRANCH" != "master" ]]; then
  fail "current branch is ${CURRENT_BRANCH} (expected master)."
fi

if ! git rev-parse --abbrev-ref --symbolic-full-name @{upstream} >/dev/null 2>&1; then
  fail "no upstream configured for master."
fi

read -r BEHIND AHEAD < <(git rev-list --left-right --count @{upstream}...HEAD)

if [[ "${BEHIND}" != "0" ]]; then
  fail "local master is behind upstream. Run git pull first."
fi

if [[ "${AHEAD}" != "0" ]]; then
  fail "local master has unpushed commits. Push or reconcile first."
fi

if git rev-parse --verify --quiet "refs/tags/${TAG}" >/dev/null; then
  fail "tag ${TAG} already exists locally."
fi

if git ls-remote --exit-code --tags origin "refs/tags/${TAG}" >/dev/null 2>&1; then
  fail "tag ${TAG} already exists on origin."
fi

if ! grep -q "## \[${VERSION}\]" CHANGELOG.md; then
  fail "CHANGELOG.md missing section for [${VERSION}]."
fi

IFS='.' read -r V_MAJOR V_MINOR V_PATCH <<< "$VERSION"

printf 'Running release-like build/test with -DINK_VERSION=%s...\n' "$VERSION"
BUILD_DIR="${INK_PREFLIGHT_BUILD_DIR:-build-release-preflight}"
cmake -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DINK_BUILD_TESTS=ON \
  -DINK_ENABLE_GL=OFF \
  -DINK_BUILD_EXAMPLES=OFF \
  -DINK_VERSION="$VERSION"
cmake --build "$BUILD_DIR" -j"$(nproc)"
ctest --test-dir "$BUILD_DIR" --output-on-failure

printf 'Checking installed generated version header...\n'
SMOKE_DIR="$(mktemp -d)"
trap 'rm -rf "$SMOKE_DIR"' EXIT
cmake --install "$BUILD_DIR" --prefix "$SMOKE_DIR/install"

cat > "$SMOKE_DIR/version_smoke.cpp" <<EOF
#include "ink/ink.hpp"

#include <cstring>

int main() {
    if (std::strcmp(ink::version(), "${VERSION}") != 0) {
        return 1;
    }
    if (ink::versionMajor() != ${V_MAJOR}) {
        return 2;
    }
    if (ink::versionMinor() != ${V_MINOR}) {
        return 3;
    }
    if (ink::versionPatch() != ${V_PATCH}) {
        return 4;
    }
    return 0;
}
EOF

INK_LIB="$(find "$SMOKE_DIR/install" -name libink.a -print -quit)"
if [[ -z "$INK_LIB" ]]; then
  fail "installed libink.a was not found."
fi

"${CXX:-c++}" -std=c++17 \
  -I "$SMOKE_DIR/install/include" \
  "$SMOKE_DIR/version_smoke.cpp" \
  "$INK_LIB" \
  -lm \
  -o "$SMOKE_DIR/version_smoke"
"$SMOKE_DIR/version_smoke"

printf 'PASS: preflight checks succeeded for %s\n' "$TAG"
