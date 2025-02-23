#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace specs {
struct vk_instance {
  std::vector<VkExtensionProperties> extensions{};
  std::vector<VkLayerProperties> layers{};
};

struct vk_queue_family {
  VkQueueFamilyProperties properties{};
  bool presentation_support{};
};

struct vk_device {
  std::vector<VkExtensionProperties> extensions{};
  std::vector<vk_queue_family> queue_families{};
  VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceFeatures features{};
};

struct vk_surface {
  std::vector<VkSurfaceFormatKHR> surface_formats;
  std::vector<VkPresentModeKHR> present_modes;
  VkSurfaceCapabilitiesKHR capabilities{};
};
} // namespace specs
