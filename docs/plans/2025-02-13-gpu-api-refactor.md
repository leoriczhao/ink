# GPU API Refactor - 模仿 Skia 设计

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 重构 GPU 相关 API，模仿 Skia 的清晰设计，同时保留 Backend 对称架构

**Architecture:**
- `GpuContext` 变成干净的抽象类，无任何后端代码
- 后端工厂函数放在单独的头文件中 (`gl_context.hpp`, `vk_context.hpp`)
- Surface 工厂简化，Backend 变成内部实现细节
- 保留 CPU/GPU 对称的 Backend 设计

**Tech Stack:** C++17, CMake, OpenGL/GLEW (可选)

---

## 参考：Skia 设计

```
include/gpu/ganesh/
├── GrDirectContext.h           # 核心 GPU 上下文类（干净）
├── gl/GrGLDirectContext.h      # GL 工厂：GrDirectContexts::MakeGL()
├── vk/GrVkDirectContext.h      # Vulkan 工厂：GrDirectContexts::MakeVulkan()
└── SkSurfaceGanesh.h           # GPU Surface 工厂

include/core/
└── SkSurface.h                 # 基础 Surface（CPU）
```

用户代码：
```cpp
// CPU
auto surface = SkSurfaces::Raster(imageInfo);

// GPU
auto ctx = GrDirectContexts::MakeGL();
auto surface = SkSurfaces::RenderTarget(ctx, budgeted, imageInfo);
```

---

## 目标：Ink 新设计

```
include/ink/
├── surface.hpp                 # Surface 工厂（CPU + 通用）
├── gpu/
│   ├── gpu_context.hpp         # GpuContext 抽象类（干净）
│   └── gl/
│       └── gl_context.hpp      # GL 工厂：GpuContexts::MakeGL()
└── internal/                   # 内部实现（用户不应该直接 include）
    └── backend.hpp             # Backend 抽象基类
```

用户代码：
```cpp
// CPU
auto surface = Surface::MakeRaster(w, h);

// GPU（需要 #include <ink/gpu/gl/gl_context.hpp>）
auto ctx = GpuContexts::MakeGL();
auto surface = Surface::MakeGpu(ctx, w, h);

// 自动选择
auto surface = Surface::MakeAuto(w, h);
```

---

## Phase 1: 清理 GpuContext 头文件

### Task 1.1: 清理 gpu_context.hpp

**Files:**
- Modify: `include/ink/gpu/gpu_context.hpp`

**目标：** 移除所有 GL 相关代码，变成干净的抽象类

**修改后内容：**

```cpp
#pragma once

#include "ink/types.hpp"
#include <memory>

namespace ink {

class Image;
class GpuImpl;

/**
 * GpuContext - Backend-agnostic shared GPU resource context.
 *
 * Owns cross-surface shared GPU resources (e.g. CPU-image texture cache).
 * Create via backend-specific factory functions:
 *   - GpuContexts::MakeGL() (include ink/gpu/gl/gl_context.hpp)
 *
 * This class is intentionally minimal. All GPU-specific logic is
 * hidden behind GpuImpl (internal).
 */
class GpuContext {
public:
    ~GpuContext();

    bool valid() const;

private:
    std::shared_ptr<GpuImpl> impl_;

    explicit GpuContext(std::shared_ptr<GpuImpl> impl);

    static std::shared_ptr<GpuContext> MakeFromImpl(std::unique_ptr<GpuImpl> impl);
    u64 resolveImageTexture(const Image* image);

    friend class GLBackend;
    friend class GpuImpl;
};

} // namespace ink
```

**Step 1: 备份当前文件**

```bash
cp include/ink/gpu/gpu_context.hpp include/ink/gpu/gpu_context.hpp.bak
```

**Step 2: 修改文件内容**

移除：
- `#if INK_HAS_GL` 块
- `friend std::shared_ptr<GpuContext> GpuContexts::MakeGL()`

**Step 3: 验证编译**

```bash
cmake --build build -j$(nproc) 2>&1
```

**Step 4: Commit**

```bash
git add include/ink/gpu/gpu_context.hpp
git commit -m "refactor: clean up GpuContext header, remove GL-specific code"
```

---

### Task 1.2: 修复 gl_context.hpp

**Files:**
- Modify: `include/ink/gpu/gl/gl_context.hpp`

**目标：** 让这个文件成为 GL 后端的唯一入口

**修改后内容：**

```cpp
#pragma once

#include "ink/gpu/gpu_context.hpp"

// This header is only usable when INK_HAS_GL is defined.
// Including it without GL support will cause a compile error.

#if !INK_HAS_GL
#error "GL backend not available. Build with -DINK_ENABLE_GL=ON"
#endif

namespace ink {

/**
 * GL-specific GpuContext factory functions.
 *
 * Usage:
 *   #include <ink/gpu/gl/gl_context.hpp>
 *   auto ctx = GpuContexts::MakeGL();
 */
namespace GpuContexts {

/**
 * Create a GpuContext bound to the currently active OpenGL context.
 * Host must have created and made current a GL context before calling.
 * Returns nullptr if no GL context is current or GL init fails.
 */
std::shared_ptr<GpuContext> MakeGL();

} // namespace GpuContexts

} // namespace ink
```

**Step 1: 修改文件内容**

**Step 2: 验证编译**

```bash
cmake --build build -j$(nproc) 2>&1
```

**Step 3: Commit**

```bash
git add include/ink/gpu/gl/gl_context.hpp
git commit -m "refactor: gl_context.hpp becomes the sole GL factory header"
```

---

### Task 1.3: 修复 gl_context.cpp

**Files:**
- Modify: `src/gpu/gl/gl_context.cpp`

**目标：** 确保实现文件正确 include 头文件

当前文件已经正确，只需要确认：
- `#include "ink/gpu/gl/gl_context.hpp"` 存在
- `#include "ink/gpu/gpu_context.hpp"` 存在

**Step 1: 验证当前内容**

```bash
head -10 src/gpu/gl/gl_context.cpp
```

**Step 2: 如果需要，添加缺失的 include**

**Step 3: 验证编译**

```bash
cmake --build build -j$(nproc) 2>&1
```

---

## Phase 2: 简化 Surface API

### Task 2.1: 修改 Surface::MakeGpu 签名

**Files:**
- Modify: `include/ink/surface.hpp`
- Modify: `src/surface.cpp`

**目标：** MakeGpu 直接接受 GpuContext，内部创建 Backend

**新的 surface.hpp 签名：**

```cpp
// GPU surface - creates internal Backend from GpuContext
// Falls back to CPU if context is nullptr
static std::unique_ptr<Surface> MakeGpu(std::shared_ptr<GpuContext> context,
                                        i32 w, i32 h,
                                        PixelFormat fmt = PixelFormat::BGRA8888);

// 保留旧的 Backend 接口作为内部使用（可选）
private:
    static std::unique_ptr<Surface> MakeGpu(std::unique_ptr<Backend> backend, i32 w, i32 h,
                                            PixelFormat fmt);
```

**Step 1: 修改 surface.hpp 声明**

**Step 2: 修改 surface.cpp 实现**

需要：
1. 新增 `MakeGpu(context, w, h, fmt)` 实现
2. 内部调用 `GLBackend::Make(context, w, h)` 创建 backend
3. 旧接口保留或移除

**Step 3: 验证编译**

```bash
cmake --build build -j$(nproc) 2>&1
```

**Step 4: Commit**

```bash
git add include/ink/surface.hpp src/surface.cpp
git commit -m "refactor: Surface::MakeGpu now accepts GpuContext directly"
```

---

### Task 2.2: 更新 example 使用新 API

**Files:**
- Modify: `examples/example_basic.cpp`
- Modify: `examples/example_composite.cpp`

**目标：** 展示简化后的 API 用法

**新的 example_basic.cpp（简化版）：**

```cpp
#include <ink/ink.hpp>
#include <ink/gpu/gl/gl_context.hpp>
#include <SDL2/SDL.h>

int main() {
    const int W = 400, H = 300;
    
    // 初始化 SDL...
    
    // 简化的 GPU Surface 创建
    auto gpuContext = GpuContexts::MakeGL();
    auto surface = Surface::MakeGpu(gpuContext, W, H);
    
    // 或者自动选择
    // auto surface = Surface::MakeAuto(W, H);
    
    // 绘制...
}
```

**Step 1: 更新 example_basic.cpp**

**Step 2: 更新 example_composite.cpp**

**Step 3: 验证编译和运行**

```bash
cmake --build build -j$(nproc)
./build/example_basic
```

**Step 4: Commit**

```bash
git add examples/
git commit -m "refactor: update examples to use simplified GPU API"
```

---

## Phase 3: 整理 Backend 层

### Task 3.1: 将 Backend 移到 internal 目录（可选）

**Files:**
- Create: `include/ink/internal/backend.hpp`
- Modify: `include/ink/backend.hpp` -> 废弃或 redirect

**目标：** Backend 变成内部实现细节

**Step 1: 创建 internal 目录**

```bash
mkdir -p include/ink/internal
```

**Step 2: 移动或复制 backend.hpp**

```bash
cp include/ink/backend.hpp include/ink/internal/backend.hpp
```

**Step 3: 修改原 backend.hpp 为转发头文件**

```cpp
#pragma once
#warning "ink/backend.hpp is deprecated, include ink/internal/backend.hpp instead"
#include "ink/internal/backend.hpp"
```

**Step 4: 更新所有 include**

```bash
# 查找所有 include backend.hpp 的文件
grep -r '#include "ink/backend.hpp"' --include="*.hpp" --include="*.cpp"
```

**Step 5: Commit**

```bash
git add include/ink/backend.hpp include/ink/internal/
git commit -m "refactor: move Backend to internal namespace"
```

---

### Task 3.2: 保留 CpuBackend 为公开 API（可选）

**决定：** CpuBackend 是否应该公开？

选项 A：公开（用户可以自己管理 Pixmap）
选项 B：内部（用户只通过 Surface API）

**建议：保留公开**，因为有些用户可能想要直接操作像素。

---

## Phase 4: 添加 Vulkan 后端框架

### Task 4.1: 创建 vk_context.hpp 框架

**Files:**
- Create: `include/ink/gpu/vk/vk_context.hpp`
- Create: `src/gpu/vk/vk_context.cpp` (空实现)

**目标：** 为未来 Vulkan 支持预留接口

**vk_context.hpp 内容：**

```cpp
#pragma once

#include "ink/gpu/gpu_context.hpp"

#if !INK_HAS_VULKAN
#error "Vulkan backend not available. Build with -DINK_ENABLE_VULKAN=ON"
#endif

namespace ink {

namespace GpuContexts {

/**
 * Create a GpuContext for Vulkan.
 * TODO: implement
 */
std::shared_ptr<GpuContext> MakeVulkan();

} // namespace GpuContexts

} // namespace ink
```

**Step 1: 创建文件**

**Step 2: 更新 CMakeLists.txt**

```cmake
if(INK_ENABLE_VULKAN)
    # 未来添加 Vulkan 源文件
    # target_sources(ink PRIVATE src/gpu/vk/vk_context.cpp)
endif()
```

**Step 3: Commit**

```bash
git add include/ink/gpu/vk/ CMakeLists.txt
git commit -m "feat: add Vulkan context header skeleton"
```

---

## Phase 5: 文档和测试

### Task 5.1: 更新 README/API 文档

**Files:**
- Modify: `README.md` 或创建 `docs/api.md`

**内容：**
- CPU Surface 用法
- GPU Surface 用法
- 后端选择

### Task 5.2: 添加单元测试

**Files:**
- Create: `tests/test_gpu_context.cpp`

**测试内容：**
- GpuContext 创建/销毁
- Surface::MakeGpu fallback 到 CPU

---

## 最终文件结构

```
include/ink/
├── ink.hpp                     # 统一入口
├── surface.hpp                 # Surface 工厂
├── canvas.hpp
├── pixmap.hpp
├── image.hpp
├── types.hpp
├── cpu_backend.hpp             # 公开（可选直接使用）
├── gpu/
│   ├── gpu_context.hpp         # 干净的 GpuContext 抽象
│   ├── gl/
│   │   ├── gl_context.hpp      # GL 工厂：GpuContexts::MakeGL()
│   │   └── gl_backend.hpp      # GL 后端实现
│   └── vk/
│       └── vk_context.hpp      # VK 工厂（未来）
└── internal/
    └── backend.hpp             # Backend 基类（内部）

src/
├── surface.cpp
├── cpu_backend.cpp
├── gpu/
│   ├── gpu_context.cpp
│   ├── gpu_impl.hpp            # 内部
│   └── gl/
│       ├── gl_context.cpp
│       ├── gl_backend.cpp
│       └── gl_gpu_impl.cpp
└── ...
```

---

## 用户代码对比

### 重构前

```cpp
#include <ink/ink.hpp>
#include <ink/gpu/gl/gl_backend.hpp>
#include <ink/gpu/gl/gl_context.hpp>

// 复杂：需要理解 Backend 层
auto gpuContext = GpuContexts::MakeGL();
auto backend = GLBackend::Make(gpuContext, W, H);
auto surface = Surface::MakeGpu(std::move(backend), W, H);
```

### 重构后

```cpp
#include <ink/ink.hpp>
#include <ink/gpu/gl/gl_context.hpp>

// 简单：只需要 Context
auto gpuContext = GpuContexts::MakeGL();
auto surface = Surface::MakeGpu(gpuContext, W, H);

// 或者更简单
auto surface = Surface::MakeAuto(W, H);  // 自动选择 GPU/CPU
```

---

## Checklist

- [ ] Phase 1: 清理 GpuContext 头文件
  - [ ] Task 1.1: 清理 gpu_context.hpp
  - [ ] Task 1.2: 修复 gl_context.hpp
  - [ ] Task 1.3: 修复 gl_context.cpp
- [ ] Phase 2: 简化 Surface API
  - [ ] Task 2.1: 修改 Surface::MakeGpu 签名
  - [ ] Task 2.2: 更新 example
- [ ] Phase 3: 整理 Backend 层（可选）
  - [ ] Task 3.1: 将 Backend 移到 internal
- [ ] Phase 4: 添加 Vulkan 框架（可选）
- [ ] Phase 5: 文档和测试
