#include "ink/version.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

std::array<int, 3> parseExpectedVersion() {
  const std::string version = INK_TEST_EXPECTED_VERSION;
  std::array<int, 3> parts{};
  std::size_t start = 0;

  for (std::size_t index = 0; index < parts.size(); ++index) {
    const std::size_t end = version.find('.', start);
    const std::string component = version.substr(start, end - start);
    parts[index] = std::stoi(component);

    if (end == std::string::npos) {
      if (index != parts.size() - 1) {
        throw std::runtime_error("expected version has too few components");
      }
      return parts;
    }

    start = end + 1;
  }

  if (start < version.size()) {
    throw std::runtime_error("expected version has too many components");
  }

  return parts;
}

} // namespace

TEST(Version, ReportsConfiguredVersionString) {
  EXPECT_STREQ(ink::version(), INK_TEST_EXPECTED_VERSION);
}

TEST(Version, ReportsConfiguredVersionComponents) {
  const auto parts = parseExpectedVersion();

  EXPECT_EQ(INK_VERSION_MAJOR, parts[0]);
  EXPECT_EQ(INK_VERSION_MINOR, parts[1]);
  EXPECT_EQ(INK_VERSION_PATCH, parts[2]);

  EXPECT_EQ(ink::versionMajor(), parts[0]);
  EXPECT_EQ(ink::versionMinor(), parts[1]);
  EXPECT_EQ(ink::versionPatch(), parts[2]);
}
