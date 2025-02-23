#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <array>
#include <glfw_adapter.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <resource.hpp>
#include <specs.hpp>
#include <string>
#include <vk_adapter.hpp>

struct vertex {
  glm::vec3 position{};
  glm::vec4 color{};

  using attr_desc_t = std::array<VkVertexInputAttributeDescription, 2>;
  static attr_desc_t attribute_description() {
    attr_desc_t desc{};
    desc[0].binding = 0;
    desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    desc[0].location = 0;
    desc[0].offset = 0;

    desc[1].binding = 0;
    desc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    desc[1].location = 1;
    desc[1].offset = sizeof(vertex::position);
    return desc;
  }
};

struct transformation {
	glm::mat4 model{1.f};
	glm::mat4 view{1.f};
	glm::mat4 projection{1.f};
};

struct frame_objects {
  VkCommandBuffer presentation_buffer{};
  VkCommandBuffer graphics_buffer{};
  raii::resource<adapter::vk_semaphore> image_available{};
  raii::resource<adapter::vk_semaphore> rendering_done{};
  raii::resource<adapter::vk_fence> presentation_done{};
  VkDescriptorSet descriptor_set{};
  raii::resource<adapter::vma_buffer> desc_buffer{};
};

struct context {
  context() = default;
  context(const context &) = delete;
  context &operator=(const context &) = delete;
  context(context &&) = default;
  context &operator=(context &&) = default;
  ~context() {
    if (device.handle)
      vkDeviceWaitIdle(device.handle);
  }

  static constexpr uint32_t concurrent_frames{2};
  uint32_t window_width{1280}, window_height{720};
  float shift_t{0.1f}, // translation
      shift_r{0.1f},   // rotation
      shift_s{0.1},    // scale
      red{0.f}, green{0.f}, blue{0.f};
  bool debug{false}, help{false}, compress{false};
  std::string matrix_file{};
  std::size_t log_level{};
  std::size_t party{};

  VkApplicationInfo app_info{};
  VkViewport viewport{};
  VkRect2D scissor{};

  raii::resource<adapter::glfw_guard> glfw_guard{};
  raii::resource<adapter::vk_instance> instance{};
  specs::vk_instance instance_capabilities{};

  raii::resource<adapter::vk_device> device{};
  specs::vk_device device_capabilities{};
  VkPhysicalDevice selected_device{};
  uint32_t presentation_queue_family_index{};
  uint32_t graphics_queue_family_index{};
  VkQueue presentation_queue{};
  VkQueue graphics_queue{};
  raii::resource<adapter::vk_memory_allocator> allocator{};

  raii::resource<adapter::glfw_window> window{};
  raii::resource<adapter::vk_surface> surface{};
  specs::vk_surface surface_capabilities{};
  raii::resource<adapter::vk_swapchain> swapchain{};
  VkSurfaceFormatKHR surface_format{};
  std::vector<VkImage> images{};
  std::vector<raii::resource<adapter::vk_image_view>> image_views{};
  std::vector<raii::resource<adapter::vma_image>> depth_images{};
  std::vector<raii::resource<adapter::vk_image_view>> depth_views{};
  VkFormat depth_format{};
  std::vector<raii::resource<adapter::vk_framebuffer>> framebuffers{};

  raii::resource<adapter::vk_render_pass> render_pass{};
  raii::resource<adapter::vk_command_pool> presentation_command_pool{};
  raii::resource<adapter::vk_command_pool> graphics_command_pool{};
	raii::resource<adapter::vk_descriptor_pool> desc_pool{};
	raii::resource<adapter::vk_descriptor_set_layout> desc_layout{};
  std::array<frame_objects, concurrent_frames> per_frame{};
  raii::resource<adapter::vk_pipeline_layout> layout{};
  raii::resource<adapter::vk_pipeline> pipeline{};

  VkBufferCreateInfo vertex_buffer_create_info{};
  raii::resource<adapter::vma_buffer> vertex_buffer{};
  std::vector<vertex> vertices{};
  transformation matrices{};
  bool update_buffers{false};
  std::size_t frame_index{};
};
