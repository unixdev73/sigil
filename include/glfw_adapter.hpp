#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace adapter {
struct glfw_guard {
  glfw_guard() = default;
  void *handle{};
  void destroy() { glfwTerminate(); }
};

struct glfw_window {
  glfw_window() = default;
  glfw_window(GLFWwindow *ptr) : handle{ptr} {}
  GLFWwindow *handle{};
  void destroy() { glfwDestroyWindow(handle); }
};
} // namespace adapter
