#pragma once

#include "ink/gpu/gpu_context.hpp"

// This header is only usable when INK_HAS_VULKAN is defined.
// Including it without Vulkan support will cause a compile error.

#if !INK_HAS_VULKAN
#error "Vulkan backend not available. Build with -DINK_ENABLE_VULKAN=ON"
#endif

namespace ink {

/**
 * Vulkan-specific GpuContext factory functions.
 *
 * Usage:
 *   #include <ink/gpu/vk/vk_context.hpp>
 *   auto ctx = GpuContexts::MakeVulkan();
 */
namespace GpuContexts {

/**
 * Create a GpuContext for Vulkan.
 * Host must have initialized Vulkan before calling.
 * Returns nullptr if Vulkan init fails.
 *
 * TODO: Implement Vulkan backend
 */
std::shared_ptr<GpuContext> MakeVulkan();

} // namespace GpuContexts

} // namespace ink
