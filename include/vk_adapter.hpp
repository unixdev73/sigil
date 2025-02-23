#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace adapter {
struct vk_instance {
  vk_instance() = default;
  vk_instance(VkInstance h) : handle{h} {}
  VkInstance handle{};
  void destroy() { vkDestroyInstance(handle, nullptr); }
};

struct vk_device {
  vk_device() = default;
  vk_device(VkDevice h) : handle{h} {}
  VkDevice handle{};
  void destroy() { vkDestroyDevice(handle, nullptr); }
};

struct vk_memory_allocator {
  vk_memory_allocator() = default;
  vk_memory_allocator(VmaAllocator h) : handle{h} {}
  VmaAllocator handle{};
  void destroy() { vmaDestroyAllocator(handle); }
};

struct vk_buffer {
  vk_buffer() = default;
  vk_buffer(VkDevice d, VkBuffer h) : device{d}, handle{h} {}
  VkDevice device{};
  VkBuffer handle{};
  void destroy() { vkDestroyBuffer(device, handle, nullptr); }
};

struct vma_buffer {
  vma_buffer() = default;
  vma_buffer(VmaAllocator a0, VmaAllocation a, VkBuffer h)
      : allocator{a0}, allocation{a}, handle{h} {}

  VmaAllocator allocator{};
  VkBuffer handle{};
  VmaAllocation allocation{};
  void destroy() { vmaDestroyBuffer(allocator, handle, allocation); }
};

struct vk_surface {
  vk_surface() = default;
  vk_surface(VkInstance i, VkSurfaceKHR h) : instance{i}, handle{h} {}
  VkInstance instance{};
  VkSurfaceKHR handle{};
  void destroy() { vkDestroySurfaceKHR(instance, handle, nullptr); }
};

struct vk_swapchain {
  vk_swapchain() = default;
  vk_swapchain(VkDevice d, VkSwapchainKHR h) : device{d}, handle{h} {}
  VkDevice device{};
  VkSwapchainKHR handle{};
  void destroy() { vkDestroySwapchainKHR(device, handle, nullptr); }
};

struct vk_render_pass {
  vk_render_pass() = default;
  vk_render_pass(VkDevice d, VkRenderPass h) : device{d}, handle{h} {}
  VkDevice device{};
  VkRenderPass handle{};
  void destroy() { vkDestroyRenderPass(device, handle, nullptr); }
};

struct vk_framebuffer {
  vk_framebuffer() = default;
  vk_framebuffer(VkDevice d, VkFramebuffer h) : device{d}, handle{h} {}
  VkDevice device{};
  VkFramebuffer handle{};
  void destroy() { vkDestroyFramebuffer(device, handle, nullptr); }
};

struct vk_image {
  vk_image() = default;
  vk_image(VkDevice d, VkImage h) : device{d}, handle{h} {}
  VkDevice device{};
  VkImage handle{};
  void destroy() { vkDestroyImage(device, handle, nullptr); }
};

struct vma_image {
  vma_image() = default;
  vma_image(VmaAllocator a0, VmaAllocation a, VkImage h)
      : allocator{a0}, allocation{a}, handle{h} {}

  VmaAllocator allocator{};
  VkImage handle{};
  VmaAllocation allocation{};
  void destroy() { vmaDestroyImage(allocator, handle, allocation); }
};

struct vk_image_view {
  vk_image_view() = default;
  vk_image_view(VkDevice d, VkImageView h) : device{d}, handle{h} {}
  VkDevice device{};
  VkImageView handle{};
  void destroy() { vkDestroyImageView(device, handle, nullptr); }
};

struct vk_semaphore {
  vk_semaphore() = default;
  vk_semaphore(VkDevice d, VkSemaphore h) : device{d}, handle{h} {}
  VkDevice device{};
  VkSemaphore handle{};
  void destroy() { vkDestroySemaphore(device, handle, nullptr); }
};

struct vk_pipeline {
  vk_pipeline() = default;
  vk_pipeline(VkDevice d, VkPipeline h) : device{d}, handle{h} {}
  VkDevice device{};
  VkPipeline handle{};
  void destroy() { vkDestroyPipeline(device, handle, nullptr); }
};

struct vk_pipeline_layout {
  vk_pipeline_layout() = default;
  vk_pipeline_layout(VkDevice d, VkPipelineLayout h) : device{d}, handle{h} {}
  VkDevice device{};
  VkPipelineLayout handle{};
  void destroy() { vkDestroyPipelineLayout(device, handle, nullptr); }
};

struct vk_command_pool {
  vk_command_pool() = default;
  vk_command_pool(VkDevice d, VkCommandPool h) : device{d}, handle{h} {}
  VkDevice device{};
  VkCommandPool handle{};
  void destroy() { vkDestroyCommandPool(device, handle, nullptr); }
};

struct vk_shader_module {
  vk_shader_module() = default;
  vk_shader_module(VkDevice d, VkShaderModule h) : device{d}, handle{h} {}
  VkDevice device{};
  VkShaderModule handle{};
  void destroy() { vkDestroyShaderModule(device, handle, nullptr); }
};

struct vk_fence {
  vk_fence() = default;
  vk_fence(VkDevice d, VkFence h) : device{d}, handle{h} {}
  VkDevice device{};
  VkFence handle{};
  void destroy() { vkDestroyFence(device, handle, nullptr); }
};

struct vk_descriptor_pool {
  vk_descriptor_pool() = default;
  vk_descriptor_pool(VkDevice d, VkDescriptorPool h) : device{d}, handle{h} {}
  VkDevice device{};
  VkDescriptorPool handle{};
  void destroy() { vkDestroyDescriptorPool(device, handle, nullptr); }
};

struct vk_descriptor_set_layout {
  vk_descriptor_set_layout() = default;
  vk_descriptor_set_layout(VkDevice d, VkDescriptorSetLayout h) :
		device{d}, handle{h} {}
  VkDevice device{};
  VkDescriptorSetLayout handle{};
  void destroy() { vkDestroyDescriptorSetLayout(device, handle, nullptr); }
};
} // namespace adapter
