#pragma once

#include <array>
#include <glfw_adapter.hpp>
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
  std::size_t log_level{};
  VkApplicationInfo app_info{};

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

  raii::resource<adapter::glfw_window> window{};
  raii::resource<adapter::vk_surface> surface{};
  specs::vk_surface surface_capabilities{};
  raii::resource<adapter::vk_swapchain> swapchain{};
  VkSurfaceFormatKHR surface_format{};
  std::vector<VkImage> images{};
  std::vector<raii::resource<adapter::vk_image_view>> image_views{};
  std::vector<raii::resource<adapter::vk_framebuffer>> framebuffers{};

  raii::resource<adapter::vk_command_pool> presentation_command_pool{};
  raii::resource<adapter::vk_command_pool> graphics_command_pool{};
  std::array<frame_objects, concurrent_frames> per_frame{};
  std::size_t frame_index{};

  raii::resource<adapter::vk_render_pass> render_pass{};
  raii::resource<adapter::vk_pipeline_layout> layout{};
  raii::resource<adapter::vk_pipeline> pipeline{};
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
