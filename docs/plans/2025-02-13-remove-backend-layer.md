# GPU API 重构 - 彻底移除 Backend 层

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 彻底移除 Backend 层，让 GpuContext 直接管理 GPU 资源，模仿 Skia 设计

**Architecture:**
- 移除 `Backend` 抽象基类
- 移除 `GpuImpl` 抽象层
- `GpuContext` 直接管理 GPU 资源（FBO、纹理、着色器）
- Surface 直接持有 `GpuContext` 或 `Pixmap`

---

## 当前问题

```
Surface
  └── Backend (多余！)
        ├── CpuBackend
        └── GLBackend
              └── GpuContext
                    └── GpuImpl (多余！)
                          └── GLGpuImpl
```

**冗余：**
- Backend 抽象 - 不需要
- GpuImpl 抽象 - 不需要
- gl_gpu_impl.cpp + gl_backend.cpp - 职责重叠

---

## 目标设计

```
Surface
  ├── Pixmap (CPU)
  └── GpuContext (GPU)
        └── GLContext (内部实现)
```

**Skia 的做法：**
```cpp
auto ctx = GrDirectContexts::MakeGL();
auto surface = SkSurfaces::RenderTarget(ctx, budgeted, imageInfo);
```

**Ink 的做法：**
```cpp
auto ctx = GpuContexts::MakeGL();
auto surface = Surface::MakeGpu(ctx, w, h);
```

---

## Phase 1: 删除 GpuImpl 层

### Task 1.1: 删除 src/gpu/gpu_impl.hpp

**Files:**
- Delete: `src/gpu/gpu_impl.hpp`

### Task 1.2: 删除 src/gpu/gl/gl_gpu_impl.cpp

**Files:**
- Delete: `src/gpu/gl/gl_gpu_impl.cpp`

### Task 1.3: 更新 src/gpu/gpu_context.cpp

移除对 GpuImpl 的依赖，GpuContext 直接管理资源。

### Task 1.4: 更新 CMakeLists.txt

移除 `gl_gpu_impl.cpp`。

---

## Phase 2: 合并 GLBackend 到 GpuContext

### Task 2.1: 将 GLBackend 的功能合并到 GLContext

**Files:**
- Modify: `src/gpu/gl/gl_context.cpp` - 加入 FBO/着色器管理
- Delete: `src/gpu/gl/gl_backend.hpp`
- Delete: `src/gpu/gl/gl_backend.cpp`

### Task 2.2: 更新 GpuContext 接口

```cpp
class GpuContext {
public:
    // 渲染接口（原 Backend 的功能）
    void execute(const Recording& recording, const DrawPass& pass);
    void beginFrame();
    void endFrame();
    void resize(i32 w, i32 h);
    std::shared_ptr<Image> makeSnapshot() const;
    void readPixels(void* dst, i32 x, i32 y, i32 w, i32 h) const;
    // ...
};
```

---

## Phase 3: 删除 Backend 抽象层

### Task 3.1: 删除 src/backend.hpp

### Task 3.2: 删除 src/cpu_backend.hpp 和 src/cpu_backend.cpp

CPU 渲染直接在 Surface 里用 Pixmap，不需要 CpuBackend。

### Task 3.3: 更新 Surface

```cpp
class Surface {
private:
    // 二选一
    std::shared_ptr<GpuContext> gpuContext_;  // GPU
    std::unique_ptr<Pixmap> pixmap_;           // CPU
};
```

---

## Phase 4: 清理

### Task 4.1: 更新 CMakeLists.txt

### Task 4.2: 更新 examples

### Task 4.3: 更新 tests

---

## 最终文件结构

```
include/ink/
├── ink.hpp
├── surface.hpp
├── canvas.hpp
├── pixmap.hpp
├── image.hpp
├── gpu/
│   ├── gpu_context.hpp      # GpuContext（含渲染接口）
│   └── gl/
│       └── gl_context.hpp   # GpuContexts::MakeGL()
└── ...

src/
├── surface.cpp              # 直接操作 Pixmap 或 GpuContext
├── canvas.cpp
├── pixmap.cpp
├── image.cpp
├── gpu/
│   ├── gpu_context.cpp      # 基础实现
│   └── gl/
│       └── gl_context.cpp   # GL 实现（原 gl_backend + gl_gpu_impl）
└── ...
```

---

## Checklist

- [ ] Phase 1: 删除 GpuImpl 层
- [ ] Phase 2: 合并 GLBackend 到 GpuContext
- [ ] Phase 3: 删除 Backend 抽象层
- [ ] Phase 4: 清理
