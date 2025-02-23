#include "sigil.hpp"
#include <chrono>
#include <logger.hpp>
#include <random>

namespace ch = std::chrono;
bool update(context *c);

namespace {
bool update_buffers(context *c);
void update_input(context *c);
void party(std::vector<vertex> &vertices, std::size_t t);
} // namespace

bool update(context *c) {
  logger l{c->log_level};
  if (c->update_buffers || c->party) {
    vkDeviceWaitIdle(c->device.handle);
    update_buffers(c);
    if (vmaCopyMemoryToAllocation(c->allocator.handle, c->vertices.data(),
                                  c->vertex_buffer.allocation, 0,
                                  c->vertex_buffer_create_info.size) !=
        VK_SUCCESS) {
      l.loge("Failed to copy vertices to buffer!\n");
      return false;
    }

    for (std::size_t i = 0; i < c->concurrent_frames; ++i) {
      if (vmaCopyMemoryToAllocation(c->allocator.handle, &c->matrices,
                                    c->per_frame[i].desc_buffer.allocation, 0,
                                    sizeof(transformation)) != VK_SUCCESS) {
        l.loge("Failed to copy matrices to buffer!\n");
        return false;
      }

      VkDescriptorBufferInfo dbi{.buffer = c->per_frame[i].desc_buffer.handle};
      dbi.offset = 0;
      dbi.range = VK_WHOLE_SIZE;

      VkWriteDescriptorSet wds{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
      wds.descriptorCount = 1;
      wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      wds.pBufferInfo = &dbi;
      wds.dstSet = c->per_frame[i].descriptor_set;
      wds.dstBinding = 0;
      wds.dstArrayElement = 0;
      vkUpdateDescriptorSets(c->device.handle, 1, &wds, 0, 0);
    }

    c->update_buffers = false;
  }

  update_input(c);

  if (c->party)
    party(c->vertices, c->party);

  return true;
}

namespace {
inline std::size_t make_random(std::size_t begin, std::size_t end) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  return begin + rng() % end;
}

void party(std::vector<vertex> &vertices, std::size_t t) {
  static auto stamp = decltype(ch::steady_clock::now()){};
  const auto now = ch::steady_clock::now();

  if (ch::duration_cast<ch::milliseconds>(now - stamp) > ch::milliseconds{t} ||
      stamp == decltype(ch::steady_clock::now()){})
    stamp = now;
  else
    return;

  for (auto &v : vertices) {
    double r = make_random(0, 256) / 255.0;
    double g = make_random(0, 256) / 255.0;
    double b = make_random(0, 256) / 255.0;
    v.color = {r, g, b, 1.f};
  }
}

bool update_rotate(context *c) {
  const auto x_axis = glm::vec3(1.f, 0.f, 0.f);
  const auto y_axis = glm::vec3(0.f, 1.f, 0.f);
  const auto z_axis = glm::vec3(0.f, 0.f, 1.f);
  const auto w = c->window.handle;
  auto &mat = c->matrices.model;

  if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS &&
      glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS) {

    mat = glm::rotate(mat, c->shift_r, x_axis);
    c->update_buffers = true;
    return true;
  } else if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS &&
             glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS) {

    mat = glm::rotate(mat, -c->shift_r, x_axis);
    c->update_buffers = true;
    return true;
  }

  else if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS &&
           glfwGetKey(w, GLFW_KEY_Y) == GLFW_PRESS) {

    mat = glm::rotate(mat, c->shift_r, y_axis);
    c->update_buffers = true;
    return true;
  } else if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS &&
             glfwGetKey(w, GLFW_KEY_Y) == GLFW_PRESS) {

    mat = glm::rotate(mat, -c->shift_r, y_axis);
    c->update_buffers = true;
    return true;
  }

  else if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS &&
           glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS) {

    mat = glm::rotate(mat, c->shift_r, z_axis);
    c->update_buffers = true;
    return true;
  } else if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS &&
             glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS) {

    mat = glm::rotate(mat, -c->shift_r, z_axis);
    c->update_buffers = true;
    return true;
  }

  else if (glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS) {

    mat = glm::mat4(1.f);
    c->update_buffers = true;
    return true;
  }

  return false;
}

void update_input(context *c) {
  const auto x_axis = glm::vec3(1.f, 0.f, 0.f);
  const auto y_axis = glm::vec3(0.f, 1.f, 0.f);
  const auto z_axis = glm::vec3(0.f, 0.f, 1.f);
  const auto w = c->window.handle;
  auto &mat = c->matrices.model;

  if (!update_rotate(c) && (glfwGetKey(w, GLFW_KEY_MINUS) == GLFW_PRESS)) {
    const auto v = 1.f - c->shift_s;
    mat = glm::scale(mat, glm::vec3(v, v, v));
    c->update_buffers = true;
  } else if (glfwGetKey(w, GLFW_KEY_EQUAL) == GLFW_PRESS) {
    const auto v = 1.f + c->shift_s;
    mat = glm::scale(mat, glm::vec3(v, v, v));
    c->update_buffers = true;
  }

  else if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS &&
           glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS) {
    mat = glm::translate(mat, glm::vec3(c->shift_t, 0.0f, 0.0f));
    c->update_buffers = true;
  } else if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS &&
             glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS) {
    mat = glm::translate(mat, glm::vec3(-c->shift_t, 0.0f, 0.0f));
    c->update_buffers = true;
  }

  else if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS &&
           glfwGetKey(w, GLFW_KEY_Y) == GLFW_PRESS) {
    mat = glm::translate(mat, glm::vec3(0.0f, -c->shift_t, 0.0f));
    c->update_buffers = true;
  } else if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS &&
             glfwGetKey(w, GLFW_KEY_Y) == GLFW_PRESS) {
    mat = glm::translate(mat, glm::vec3(0.0f, c->shift_t, 0.0f));
    c->update_buffers = true;
  }

  else if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS &&
           glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS) {
    mat = glm::translate(mat, glm::vec3(0.0f, 0.f, -c->shift_t));
    c->update_buffers = true;
  } else if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS &&
             glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS) {
    mat = glm::translate(mat, glm::vec3(0.0f, 0.f, c->shift_t));
    c->update_buffers = true;
  }
}

bool update_buffers(context *c) {
  auto &vb = c->vertex_buffer_create_info;
  auto a0 = c->allocator.handle;
  logger l{c->log_level};

  const auto element_size = sizeof(decltype(c->vertices)::value_type);
  const auto current_size = c->vertices.size() * element_size;

  if (current_size <= vb.size)
    return true;

  vb.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vb.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vb.size = current_size;
  vb.usage =
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VmaAllocationCreateInfo aci{};
  VkBuffer vbuf{}, ibuf{};
  VmaAllocation valloc{}, ialloc{};
  VmaAllocationInfo vai{}, iai{};

  aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

  if (vmaCreateBuffer(a0, &vb, &aci, &vbuf, &valloc, &vai) != VK_SUCCESS) {
    l.loge("Failed to create vk buffer using VMA\n");
    return false;
  }

  c->vertex_buffer = raii::resource<adapter::vma_buffer>{a0, valloc, vbuf};
  return true;
}
} // namespace
