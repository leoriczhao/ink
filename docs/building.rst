Building
========

Quick Start
-----------

.. code-block:: bash

   cmake -B build
   cmake --build build

Build Options
-------------

.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Description
   * - ``INK_ENABLE_GL``
     - ``ON``
     - Enable OpenGL backend
   * - ``INK_ENABLE_VULKAN``
     - ``OFF``
     - Enable Vulkan backend
   * - ``INK_BUILD_TESTS``
     - ``OFF``
     - Build test suite
   * - ``INK_BUILD_EXAMPLES``
     - ``OFF``
     - Build examples
   * - ``BUILD_SHARED_LIBS``
     - ``OFF``
     - Build as shared library

Build Examples
--------------

.. code-block:: bash

   # CPU-only build (no GPU dependencies)
   cmake -B build -DINK_ENABLE_GL=OFF

   # With OpenGL
   cmake -B build -DINK_ENABLE_GL=ON

   # Build with tests
   cmake -B build -DINK_BUILD_TESTS=ON
   cmake --build build
   ctest --test-dir build

   # Build with examples
   cmake -B build -DINK_BUILD_EXAMPLES=ON
   cmake --build build

Integration
-----------

As a CMake subdirectory
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cmake

   add_subdirectory(third_party/ink)
   target_link_libraries(my_app PRIVATE ink)

Manual
^^^^^^

Add ``include/`` to your include path and link against the built library.

Requirements
------------

- C++17 compiler (GCC, Clang, or MSVC)
- CMake 3.16+
- **Optional:** OpenGL + GLEW (for GL backend)
- **Test-only:** Google Test (fetched automatically via CMake FetchContent)
