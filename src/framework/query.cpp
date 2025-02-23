#include <query.hpp>

using common::result;

namespace query {
result instance_specs(specs::vk_instance *s) {
  if (!s)
    return result::domain_error;

  auto r = query::available_instance_extensions(&s->extensions);
  if (r != result::success)
    return result::query_error;

  r = query::available_instance_layers(&s->layers);
  if (r != result::success)
    return result::query_error;

  return result::success;
}

result device_specs(specs::vk_device *s, const VkInstance instance,
                    const VkPhysicalDevice device) {

  if (!s || device == VK_NULL_HANDLE)
    return result::domain_error;

  uint32_t count{};
  vkEnumerateDeviceExtensionProperties(device, 0, &count, 0);
  s->extensions.resize(count);
  vkEnumerateDeviceExtensionProperties(device, 0, &count, s->extensions.data());

  vkGetPhysicalDeviceProperties(device, &s->properties);
  vkGetPhysicalDeviceFeatures(device, &s->features);

  std::vector<VkQueueFamilyProperties> qfp{};
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, 0);
  qfp.resize(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, qfp.data());

  s->queue_families.resize(qfp.size());
  for (std::size_t i = 0; i < qfp.size(); ++i) {
    s->queue_families[i].properties = qfp[i];
    if (glfwGetPhysicalDevicePresentationSupport(instance, device, i))
      s->queue_families[i].presentation_support = true;
  }

  return result::success;
}

result surface_specs(specs::vk_surface *s, const VkPhysicalDevice device,
                     const VkSurfaceKHR surface) {

  if (!s || device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
    return result::domain_error;

  auto r = query::surface_formats(&s->surface_formats, device, surface);
  if (r != result::success)
    return result::query_error;

  r = query::surface_present_modes(&s->present_modes, device, surface);
  if (r != result::success)
    return result::query_error;

  r = query::surface_capabilities(&s->capabilities, device, surface);
  if (r != result::success)
    return result::query_error;

  return result::success;
}

result surface_formats(std::vector<VkSurfaceFormatKHR> *surface_formats,
                       const VkPhysicalDevice device,
                       const VkSurfaceKHR surface) {

  if (!surface_formats || device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
    return result::domain_error;

  uint32_t count{};

  auto r = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, 0);
  surface_formats->resize(count);
  r = vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count,
                                           surface_formats->data());

  if (r != VK_SUCCESS)
    return result::query_error;

  return result::success;
}

result surface_present_modes(std::vector<VkPresentModeKHR> *present_modes,
                             const VkPhysicalDevice device,
                             const VkSurfaceKHR surface) {

  if (!present_modes || device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
    return result::domain_error;

  uint32_t cnt{};
  auto r = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &cnt, 0);
  present_modes->resize(cnt);
  r = vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &cnt,
                                                present_modes->data());

  if (r != VK_SUCCESS)
    return result::query_error;

  return result::success;
}

result surface_capabilities(VkSurfaceCapabilitiesKHR *caps,
                            const VkPhysicalDevice device,
                            const VkSurfaceKHR surface) {

  if (!caps || device == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
    return result::domain_error;

  auto r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, caps);
  if (r != VK_SUCCESS)
    return result::query_error;

  return result::success;
}

result
available_instance_extensions(std::vector<VkExtensionProperties> *extensions) {

  if (!extensions)
    return result::domain_error;

  uint32_t count{};

  auto r = vkEnumerateInstanceExtensionProperties(0, &count, 0);
  extensions->resize(count);
  r = vkEnumerateInstanceExtensionProperties(0, &count, extensions->data());

  if (r != VK_SUCCESS)
    return result::query_error;

  return result::success;
}

result available_instance_layers(std::vector<VkLayerProperties> *layers) {
  if (!layers)
    return result::domain_error;

  uint32_t count{};

  auto r = vkEnumerateInstanceLayerProperties(&count, nullptr);
  layers->resize(count);
  r = vkEnumerateInstanceLayerProperties(&count, layers->data());
  if (r != VK_SUCCESS)
    return result::query_error;

  return result::success;
}

result available_devices(std::vector<VkPhysicalDevice> *devices,
                         const VkInstance instance) {

  if (!devices || instance == VK_NULL_HANDLE)
    return result::domain_error;

  uint32_t count{};
  vkEnumeratePhysicalDevices(instance, &count, nullptr);

  if (!count)
    return result::range_error;

  devices->resize(count);

  auto r = vkEnumeratePhysicalDevices(instance, &count, devices->data());
  if (r != VK_SUCCESS)
    return result::query_error;

  return result::success;
}
} // namespace query
