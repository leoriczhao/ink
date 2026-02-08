cmake_minimum_required(VERSION 3.16)
project(ink VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Build options ---
option(INK_ENABLE_GL      "Enable OpenGL backend"      ON)
option(INK_ENABLE_VULKAN  "Enable Vulkan backend"      OFF)
option(INK_BUILD_TESTS    "Build tests"                OFF)
option(INK_BUILD_EXAMPLES "Build examples"             OFF)
option(BUILD_SHARED_LIBS  "Build as shared library"    OFF)

# --- Core source files (always compiled) ---
set(INK_CORE_SOURCES
    src/pixmap.cpp
    src/recording.cpp
    src/draw_pass.cpp
    src/device.cpp
    src/canvas.cpp
    src/surface.cpp
    src/image.cpp
    src/cpu_backend.cpp
    src/glyph_cache.cpp
)

# --- Create library ---
add_library(ink ${INK_CORE_SOURCES})

target_include_directories(ink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# --- OpenGL backend ---
if(INK_ENABLE_GL)
    find_package(OpenGL QUIET)
    if(OpenGL_FOUND)
        target_sources(ink PRIVATE
            src/gpu/gl/gl_backend.cpp
        )
        target_link_libraries(ink PRIVATE OpenGL::GL)
        target_compile_definitions(ink PUBLIC INK_HAS_GL=1)
        message(STATUS "ink: OpenGL backend enabled")
    else()
        message(STATUS "ink: OpenGL not found, GL backend disabled")
    endif()
endif()

# --- Vulkan backend (future) ---
if(INK_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_sources(ink PRIVATE
            src/gpu/vk/vk_backend.cpp
        )
        target_link_libraries(ink PRIVATE Vulkan::Vulkan)
        target_compile_definitions(ink PUBLIC INK_HAS_VULKAN=1)
        message(STATUS "ink: Vulkan backend enabled")
    else()
        message(STATUS "ink: Vulkan not found, Vulkan backend disabled")
    endif()
endif()

# --- Platform-specific ---
if(UNIX AND NOT APPLE)
    target_link_libraries(ink PRIVATE m)
endif()

# --- Compiler warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ink PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Install ---
include(GNUInstallDirs)

install(TARGETS ink
    EXPORT inkTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ink
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT inkTargets
    FILE inkTargets.cmake
    NAMESPACE ink::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ink
)
