cmake_minimum_required(VERSION 3.16)
project(ink VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Build options ---
option(INK_ENABLE_GL      "Enable OpenGL backend"      OFF)
option(INK_ENABLE_METAL   "Enable Metal backend"       OFF)
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
    src/glyph_cache.cpp
    src/gpu/gpu_context.cpp
)

# --- Create library ---
add_library(ink ${INK_CORE_SOURCES})

target_include_directories(ink
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# --- OpenGL backend ---
if(INK_ENABLE_GL)
    find_package(OpenGL QUIET)
    find_package(GLEW QUIET)
    if(OpenGL_FOUND AND GLEW_FOUND)
        target_sources(ink PRIVATE
            src/gpu/gl/gl_context.cpp
        )
        target_link_libraries(ink PUBLIC OpenGL::GL GLEW::GLEW)
        target_compile_definitions(ink PUBLIC INK_HAS_GL=1)
        message(STATUS "ink: OpenGL backend enabled")
    else()
        if(NOT OpenGL_FOUND)
            message(STATUS "ink: OpenGL not found, GL backend disabled")
        endif()
        if(NOT GLEW_FOUND)
            message(STATUS "ink: GLEW not found, GL backend disabled")
        endif()
    endif()
endif()

# --- Metal backend (Apple only) ---
if(INK_ENABLE_METAL AND APPLE)
    enable_language(OBJCXX)
    target_sources(ink PRIVATE
        src/gpu/metal/metal_context.mm
    )
    target_link_libraries(ink PUBLIC
        "-framework Metal"
        "-framework Foundation"
    )
    target_compile_definitions(ink PUBLIC INK_HAS_METAL=1)
    set_source_files_properties(src/gpu/metal/metal_context.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
    message(STATUS "ink: Metal backend enabled")
elseif(INK_ENABLE_METAL AND NOT APPLE)
    message(STATUS "ink: Metal backend requires Apple platform, disabled")
endif()

# --- Vulkan backend (future) ---
if(INK_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
        target_sources(ink PRIVATE
            src/gpu/vk/vk_context.cpp
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

# --- Examples ---
if(INK_BUILD_EXAMPLES)
    find_package(SDL2 REQUIRED)

    add_executable(example_basic examples/example_basic.cpp)
    target_link_libraries(example_basic PRIVATE ink SDL2::SDL2)

    add_executable(example_composite examples/example_composite.cpp)
    target_link_libraries(example_composite PRIVATE ink SDL2::SDL2)

    # GPU example - requires at least one GPU backend
    set(INK_HAS_GPU_BACKEND FALSE)

    if(INK_ENABLE_METAL AND APPLE)
        set(INK_HAS_GPU_BACKEND TRUE)
    endif()
    if(INK_ENABLE_GL AND OpenGL_FOUND AND GLEW_FOUND)
        set(INK_HAS_GPU_BACKEND TRUE)
    endif()

    if(INK_HAS_GPU_BACKEND)
        add_executable(example_gpu examples/example_gpu.cpp)
        target_link_libraries(example_gpu PRIVATE ink SDL2::SDL2)

        if(INK_ENABLE_GL AND OpenGL_FOUND AND GLEW_FOUND)
            target_link_libraries(example_gpu PRIVATE OpenGL::GL GLEW::GLEW)
        endif()
    endif()
endif()

# --- Tests ---
if(INK_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    set(INK_TEST_SOURCES
        tests/test_types.cpp
        tests/test_pixmap.cpp
        tests/test_recording.cpp
        tests/test_draw_pass.cpp
        tests/test_image.cpp
        tests/test_canvas.cpp
        tests/test_surface.cpp
        tests/test_composite.cpp
    )

    add_executable(ink_tests ${INK_TEST_SOURCES})
    target_link_libraries(ink_tests PRIVATE ink GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(ink_tests)
endif()

# --- Documentation ---
find_package(Doxygen QUIET)
find_program(SPHINX_BUILD sphinx-build)

if(DOXYGEN_FOUND AND SPHINX_BUILD)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        COMMAND ${SPHINX_BUILD} -b html
                ${CMAKE_SOURCE_DIR}/docs
                ${CMAKE_SOURCE_DIR}/docs/_build/sphinx
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating documentation (Doxygen + Sphinx)"
        VERBATIM
    )
elseif(DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating Doxygen documentation (sphinx-build not found)"
        VERBATIM
    )
endif()
