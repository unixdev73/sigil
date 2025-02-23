#pragma once

#include <glfw_adapter.hpp>
#include <result.hpp>
#include <specs.hpp>

namespace query {
common::result instance_specs(specs::vk_instance *s);

common::result device_specs(specs::vk_device *s, const VkInstance instance,
                            const VkPhysicalDevice device);

common::result surface_specs(specs::vk_surface *s,
                             const VkPhysicalDevice device,
                             const VkSurfaceKHR surface);

common::result
available_instance_extensions(std::vector<VkExtensionProperties> *extensions);

common::result
available_instance_layers(std::vector<VkLayerProperties> *layers);

common::result available_devices(std::vector<VkPhysicalDevice> *devices,
                                 const VkInstance instance);

common::result surface_formats(std::vector<VkSurfaceFormatKHR> *surface_formats,
                               const VkPhysicalDevice device,
                               const VkSurfaceKHR surface);

common::result
surface_present_modes(std::vector<VkPresentModeKHR> *present_modes,
                      const VkPhysicalDevice device,
                      const VkSurfaceKHR surface);

common::result surface_capabilities(VkSurfaceCapabilitiesKHR *caps,
                                    const VkPhysicalDevice device,
                                    const VkSurfaceKHR surface);
} // namespace query
