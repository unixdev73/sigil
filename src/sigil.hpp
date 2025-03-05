#pragma once

#include <array>
#include <glfw_adapter.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <resource.hpp>
#include <specs.hpp>
#include <vk_adapter.hpp>

struct frame_objects {
  VkCommandBuffer presentation_buffer{};
  VkCommandBuffer graphics_buffer{};
  raii::resource<adapter::vk_semaphore> image_available{};
  raii::resource<adapter::vk_semaphore> rendering_done{};
  raii::resource<adapter::vk_fence> presentation_done{};
	VkDescriptorSet descriptor_set{};
	raii::resource<adapter::vma_buffer> desc_buffer{};
};

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

struct context {
  context() = default;
  context(const context &) = delete;
  context &operator=(const context &) = delete;
  context(context &&) = default;
  context &operator=(context &&) = default;
  ~context() { vkDeviceWaitIdle(device.handle); }

  static constexpr uint32_t concurrent_frames{2};
  uint32_t window_width{1280}, window_height{720};
  bool debug{false}, help{false};
  std::string matrix_source_file{};
  std::size_t log_level{};
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
  raii::resource<adapter::vma_buffer> index_buffer{};
  std::vector<vertex> vertices{};
	std::vector<uint32_t> indices{};
	transformation matrices{};
  bool update_buffers{false};
  std::size_t frame_index{};
};

class logger {
  template <typename... P>
    requires(sizeof...(P) >= 1)
  void log_impl(std::ostream &o, P &&...args) {
    ((o << args), ...);
  }

  void set(std::size_t level, bool v) {
    if (v)
      log_level_ |= level;
    else
      log_level_ &= ~level;
  }

  std::size_t log_level_{};

public:
  static constexpr std::size_t inf{1}, wrn{1 << 1}, err{1 << 2};
  logger(std::size_t v = logger::err) : log_level_{v} {}

  logger(const logger &) = default;
  logger &operator=(const logger &) = default;
  logger(logger &&) = default;
  logger &operator=(logger &&) = default;
  ~logger() = default;

  template <std::size_t lvl> void set(bool v) {
    static_assert(lvl == inf || lvl == wrn || lvl == err);
    this->set(lvl, v);
  }

  template <typename... P> void logi(P &&...args) {
    if (log_level_ & inf)
      log_impl(std::cout, "(INF): ", std::forward<P>(args)...);
  }

  template <typename... P> void logs(P &&...args) {
    if (log_level_ & inf)
      log_impl(std::cout, std::forward<P>(args)...);
  }

  template <typename... P> void logw(P &&...args) {
    if (log_level_ & wrn)
      log_impl(std::cout, "(WRN): ", std::forward<P>(args)...);
  }

  template <typename... P> void loge(P &&...args) {
    if (log_level_ & err)
      log_impl(std::cerr, "(ERR): ", std::forward<P>(args)...);
  }
};
