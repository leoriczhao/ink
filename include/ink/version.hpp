#pragma once

/// @file version.hpp
/// @brief Library version information.

#define INK_VERSION_MAJOR 0
#define INK_VERSION_MINOR 2
#define INK_VERSION_PATCH 0

namespace ink {

/// @brief Return the library version string (e.g. "0.2.0").
/// @return Null-terminated version string in "major.minor.patch" format.
inline const char* version() {
    return "0.2.0";
}

/// @brief Return the major version number.
/// @return Major version as an integer.
inline int versionMajor() { return INK_VERSION_MAJOR; }
/// @brief Return the minor version number.
/// @return Minor version as an integer.
inline int versionMinor() { return INK_VERSION_MINOR; }
/// @brief Return the patch version number.
/// @return Patch version as an integer.
inline int versionPatch() { return INK_VERSION_PATCH; }

} // namespace ink
