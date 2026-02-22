Building ink
============

Requirements
------------

- C++17 compiler (GCC, Clang, or MSVC)
- CMake 3.16+
- **Optional:** OpenGL + GLEW (for GL backend)
- **Optional:** Metal framework (macOS, for Metal backend)
- **Test-only:** Google Test (fetched automatically via CMake FetchContent)

Build Options
-------------

.. list-table::
   :header-rows: 1

   * - Option
     - Default
     - Description
   * - ``INK_ENABLE_GL``
     - ``ON``
     - Enable OpenGL 3.3+ backend
   * - ``INK_ENABLE_METAL``
     - ``OFF``
     - Enable Metal backend (macOS only)
   * - ``INK_ENABLE_VULKAN``
     - ``OFF``
     - Enable Vulkan backend (planned)
   * - ``INK_BUILD_TESTS``
     - ``OFF``
     - Build the Google Test suite
   * - ``INK_BUILD_EXAMPLES``
     - ``OFF``
     - Build example programs
   * - ``BUILD_SHARED_LIBS``
     - ``OFF``
     - Build as shared library

Common Build Configurations
---------------------------

**CPU-only (no GPU dependencies):**

.. code-block:: bash

   cmake -B build -DINK_ENABLE_GL=OFF
   cmake --build build -j$(nproc)

**With OpenGL and tests:**

.. code-block:: bash

   cmake -B build -DINK_ENABLE_GL=ON -DINK_BUILD_TESTS=ON
   cmake --build build -j$(nproc)
   ctest --test-dir build --output-on-failure

**With examples:**

.. code-block:: bash

   cmake -B build -DINK_BUILD_EXAMPLES=ON
   cmake --build build -j$(nproc)

Integration
-----------

**As a CMake subdirectory:**

.. code-block:: cmake

   add_subdirectory(third_party/ink)
   target_link_libraries(my_app PRIVATE ink)

**Manual:** Add ``include/`` to your include path and link against the built
library (``libink.a`` or ``ink.lib``).

Building Documentation
----------------------

To generate the full documentation (Doxygen API reference + Sphinx guides):

.. code-block:: bash

   cmake -B build
   cmake --build build --target docs

This runs Doxygen first (producing XML + standalone HTML), then Sphinx
(producing the combined documentation site). Output is in
``docs/_build/sphinx/``.

You can also run the tools manually:

.. code-block:: bash

   # Doxygen only
   doxygen Doxyfile

   # Sphinx only (requires Doxygen XML to exist)
   sphinx-build docs docs/_build/sphinx
