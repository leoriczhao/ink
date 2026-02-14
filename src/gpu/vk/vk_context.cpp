// Vulkan context factory - creates a GpuContext for Vulkan.
//
// Only compiled when INK_HAS_VULKAN is defined (via CMake).
// TODO: Implement Vulkan backend

#include "ink/gpu/vk/vk_context.hpp"
#include "ink/gpu/gpu_context.hpp"
#include "gpu/gpu_impl.hpp"

namespace ink {

// TODO: Implement Vulkan GpuImpl
// std::unique_ptr<GpuImpl> makeVulkanGpuImpl();

namespace GpuContexts {

std::shared_ptr<GpuContext> MakeVulkan() {
    // TODO: Implement
    // auto impl = makeVulkanGpuImpl();
    // return GpuImpl::createContext(std::move(impl));
    return nullptr;
}

} // namespace GpuContexts
} // namespace ink
