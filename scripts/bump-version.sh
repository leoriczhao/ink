#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  printf 'Usage: %s <version>\n' "$0"
  printf 'Example: %s 0.2.0\n' "$0"
  exit 1
fi

VERSION="$1"
IFS='.' read -r V_MAJOR V_MINOR V_PATCH <<< "$VERSION"

if [[ -z "$V_MAJOR" || -z "$V_MINOR" || -z "$V_PATCH" ]]; then
  printf 'ERROR: version must be in X.Y.Z format.\n'
  exit 1
fi

printf 'Bumping all version references to %s ...\n' "$VERSION"

# 1. CMakeLists.txt
sed -i "s/project(ink VERSION [0-9]\+\.[0-9]\+\.[0-9]\+/project(ink VERSION ${VERSION}/" CMakeLists.txt
printf '  CMakeLists.txt  ✓\n'

# 2. include/ink/version.hpp
sed -i "s/#define INK_VERSION_MAJOR [0-9]\+/#define INK_VERSION_MAJOR ${V_MAJOR}/" include/ink/version.hpp
sed -i "s/#define INK_VERSION_MINOR [0-9]\+/#define INK_VERSION_MINOR ${V_MINOR}/" include/ink/version.hpp
sed -i "s/#define INK_VERSION_PATCH [0-9]\+/#define INK_VERSION_PATCH ${V_PATCH}/" include/ink/version.hpp
sed -i "s/return \"[0-9]\+\.[0-9]\+\.[0-9]\+\"/return \"${VERSION}\"/" include/ink/version.hpp
sed -i "s/(e\.g\. \"[0-9]\+\.[0-9]\+\.[0-9]\+\")/(e.g. \"${VERSION}\")/" include/ink/version.hpp
printf '  include/ink/version.hpp  ✓\n'

# 3. Doxyfile
sed -i "s/PROJECT_NUMBER.*=.*/PROJECT_NUMBER         = \"${VERSION}\"/" Doxyfile
printf '  Doxyfile  ✓\n'

# 4. docs/conf.py
sed -i "s/release = '[0-9]\+\.[0-9]\+\.[0-9]\+'/release = '${VERSION}'/" docs/conf.py
printf '  docs/conf.py  ✓\n'

printf '\nDone. All version references updated to %s.\n' "$VERSION"
printf 'Remember to also update CHANGELOG.md manually.\n'
