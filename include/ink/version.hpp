#pragma once

// Version information
#define INK_VERSION_MAJOR 0
#define INK_VERSION_MINOR 1
#define INK_VERSION_PATCH 0

namespace ink {

// Library information
inline const char* version() {
    return "0.1.0";
}

inline int versionMajor() { return INK_VERSION_MAJOR; }
inline int versionMinor() { return INK_VERSION_MINOR; }
inline int versionPatch() { return INK_VERSION_PATCH; }

} // namespace ink
