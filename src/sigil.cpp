#include "sigil.hpp"
#include <cfgtk/lexer.hpp>
#include <cfgtk/parser.hpp>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <query.hpp>
#include <random>
#include <shader.hpp>

namespace fs = std::filesystem;

namespace {
bool initialize(context *, const fs::path, const std::vector<std::string>);
void initialize_dynamic_state(context *c);
bool parse_cli(context *c, const std::vector<std::string> *);
bool initialize_glfw(context *c);
bool create_instance(context *c);
bool select_physical(context *c);
bool create_device(context *c);
bool create_memory_allocator(context *c);
bool create_window(context *c);
bool create_surface(context *c);
bool create_swapchain(context *c);
bool create_image_views(context *c);
bool create_depth_images(context *c);
bool create_framebuffers(context *c);
bool create_render_pass(context *c);
bool create_descriptor_pool(context* c);
bool create_pipeline_layout(context* c);
bool create_pipeline(context *c);
bool create_semaphores(context *c);
bool create_buffers(context *c);
bool update_buffers(context *c);

bool is_available(const std::vector<VkExtensionProperties> *,
                  const std::string &);
bool is_available(const std::vector<VkLayerProperties> *, const std::string &);

bool find_supported_format(const std::vector<VkFormat> &candidates,
                           const VkPhysicalDevice dev, VkImageTiling tiling,
                           VkFormatFeatureFlags features, VkFormat *out);

bool render(context *c);
bool update(context *c);

using vtype = int;
std::vector<vertex> normalize_matrix(const std::vector<std::vector<vtype>> &);
bool configure_sigil_vertices(context *c);
bool read_matrix(const std::string &, std::vector<std::vector<vtype>> *);
} // namespace

int main(int c, char **v) {
  context ctx{};
  logger l{logger::err};

  if (!initialize(&ctx, v[0], flt::to_container<std::vector>(c, v, 1))) {
    l.loge("Initialization failed\n");
    return 1;
  }
  l = logger{ctx.log_level};

  auto start = std::chrono::steady_clock::now();
  std::array<std::size_t, 100> frame_times{};
  std::size_t index{};

  while (glfwWindowShouldClose(ctx.window.handle) != GLFW_TRUE) {
    glfwPollEvents();

    if (!update(&ctx)) {
      l.loge("Updating failed\n");
      return 2;
    }

    if (!render(&ctx)) {
      l.loge("Rendering failed\n");
      return 2;
    }

    auto end = std::chrono::steady_clock::now();
    using tu = std::chrono::nanoseconds;
    frame_times[index] = std::chrono::duration_cast<tu>(end - start).count();

    start = end;
    index = (index + 1) % frame_times.size();
  }

  std::size_t average_fps{}, offset{5};
  for (std::size_t i = offset; i < frame_times.size() - offset; ++i)
    average_fps += frame_times[i];
  average_fps /= (frame_times.size() - 2 * offset);

  std::cout << "AVG FPS: " << (1.0 / (average_fps / 1'000'000'000.0)) << "\n";
  return 0;
}

namespace {
std::vector<vertex> normalize_matrix(const std::vector<std::vector<vtype>> &m) {
  std::vector<vertex> out{};

  struct sort_info {
    std::size_t row, col;
    vtype val;
  };
  std::vector<sort_info> ordered{};
  for (std::size_t i = 0; i < m.size(); ++i)
    for (std::size_t j = 0; j < m[i].size(); ++j)
      ordered.push_back({i, j, m[i][j]});

  std::sort(ordered.begin(), ordered.end(),
            [](const auto &a, const auto &b) { return a.val < b.val; });

  double depth_max{};
  for (const auto &e : ordered)
    if (e.val > depth_max)
      depth_max = e.val;

  for (auto &&e : ordered) {
    out.push_back({.position = {e.col / double(m.size()) - 0.25,
                                e.row / double(m.size()) - 0.25,
                                double(e.val) / depth_max},
                   .color = {1.f, 1.f, 0.f, 1.f}});
  }
  return out;
}

bool find_supported_format(const std::vector<VkFormat> &candidates,
                           const VkPhysicalDevice dev, VkImageTiling tiling,
                           VkFormatFeatureFlags features, VkFormat *out) {

  for (VkFormat format : candidates) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(dev, format, &props);

    if (tiling == VK_IMAGE_TILING_LINEAR &&
        (props.linearTilingFeatures & features) == features) {
      *out = format;
      return true;
    } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
               (props.optimalTilingFeatures & features) == features) {
      *out = format;
      return true;
    }
  }

  return false;
}

bool create_depth_images(context *c) {
  logger l{c->log_level};
  if (!find_supported_format(
          {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
           VK_FORMAT_D24_UNORM_S8_UINT},
          c->selected_device, VK_IMAGE_TILING_OPTIMAL,
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, &c->depth_format)) {
    l.loge("Failed to find depth attachment format!\n");
    return false;
  }

  VkImageCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  info.imageType = VK_IMAGE_TYPE_2D;
  info.arrayLayers = 1;
  info.extent = {c->window_width, c->window_height, 1};
  info.format = c->depth_format;
  info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  info.mipLevels = 1;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  info.samples = VK_SAMPLE_COUNT_1_BIT;

  VkImageViewCreateInfo vinf{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  vinf.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vinf.format = c->depth_format;
  vinf.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  vinf.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  vinf.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  vinf.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  vinf.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  vinf.subresourceRange.baseArrayLayer = 0;
  vinf.subresourceRange.baseMipLevel = 0;
  vinf.subresourceRange.layerCount = 1;
  vinf.subresourceRange.levelCount = 1;

  c->depth_images.resize(c->images.size());
  c->depth_views.resize(c->images.size());
  const auto a0 = c->allocator.handle;
  const VkDevice dev = c->device.handle;

  for (std::size_t i = 0; i < c->depth_images.size(); ++i) {
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    aci.priority = 1.f;
    VmaAllocation alloc{};

    VkImageView img_view{};
    VkImage img{};
    if (vmaCreateImage(a0, &info, &aci, &img, &alloc, 0) != VK_SUCCESS) {
      l.loge("Failed to create depth image\n");
      return false;
    }
    c->depth_images[i] = raii::resource<adapter::vma_image>{a0, alloc, img};

    vinf.image = c->depth_images[i].handle;
    auto r = vkCreateImageView(dev, &vinf, nullptr, &img_view);
    if (r != VK_SUCCESS) {
      l.loge("Failed to create depth view\n");
      return false;
    }
    c->depth_views[i] = raii::resource<adapter::vk_image_view>{dev, img_view};
  }

  return true;
}

inline std::size_t make_random(std::size_t begin, std::size_t end) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  return begin + rng() % end;
}

void update_view(context *c) {
  static auto stamp = std::chrono::steady_clock::now();
  auto n = std::chrono::steady_clock::now();
  auto dif = std::chrono::duration_cast<std::chrono::milliseconds>(n - stamp);
  if (dif.count() >= 100)
    stamp = n;
  else
    return;

  if (glfwGetKey(c->window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS &&
      glfwGetKey(c->window.handle, GLFW_KEY_X) == GLFW_PRESS) {
    c->matrices.model =
        glm::rotate(c->matrices.model, 0.2f, glm::vec3(1.f, 0.f, 0.f));
    c->update_buffers = true;
  } else if (glfwGetKey(c->window.handle, GLFW_KEY_LEFT) == GLFW_PRESS &&
             glfwGetKey(c->window.handle, GLFW_KEY_X) == GLFW_PRESS) {
    c->matrices.model =
        glm::rotate(c->matrices.model, -0.2f, glm::vec3(1.f, 0.f, 0.f));
    c->update_buffers = true;
  }

  else if (glfwGetKey(c->window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS &&
           glfwGetKey(c->window.handle, GLFW_KEY_Y) == GLFW_PRESS) {
    c->matrices.model =
        glm::rotate(c->matrices.model, 0.2f, glm::vec3(0.f, 1.f, 0.f));
    c->update_buffers = true;
  } else if (glfwGetKey(c->window.handle, GLFW_KEY_LEFT) == GLFW_PRESS &&
             glfwGetKey(c->window.handle, GLFW_KEY_Y) == GLFW_PRESS) {
    c->matrices.model =
        glm::rotate(c->matrices.model, -0.2f, glm::vec3(0.f, 1.f, 0.f));
    c->update_buffers = true;
  }

  else if (glfwGetKey(c->window.handle, GLFW_KEY_RIGHT) == GLFW_PRESS &&
           glfwGetKey(c->window.handle, GLFW_KEY_Z) == GLFW_PRESS) {
    c->matrices.model =
        glm::rotate(c->matrices.model, 0.2f, glm::vec3(0.f, 0.f, 1.f));
    c->update_buffers = true;
  } else if (glfwGetKey(c->window.handle, GLFW_KEY_LEFT) == GLFW_PRESS &&
             glfwGetKey(c->window.handle, GLFW_KEY_Z) == GLFW_PRESS) {
    c->matrices.model =
        glm::rotate(c->matrices.model, -0.2f, glm::vec3(0.f, 0.f, 1.f));
    c->update_buffers = true;
  }
}

bool update(context *c) {
  logger l{c->log_level};
  if (c->update_buffers) {
    vkDeviceWaitIdle(c->device.handle);
    update_buffers(c);
    if (vmaCopyMemoryToAllocation(c->allocator.handle, c->vertices.data(),
                                  c->vertex_buffer.allocation, 0,
                                  c->vertex_buffer_create_info.size) !=
        VK_SUCCESS) {
      l.loge("Failed to copy vertices to buffer!\n");
      return false;
    }

    /*
if (vmaCopyMemoryToAllocation(c->allocator.handle, c->indices.data(),
                      c->index_buffer.allocation, 0,
                            sizeof(decltype(c->indices)::value_type) *
c->indices.size()) != VK_SUCCESS) { l.loge("Failed to copy indices to
buffer!\n"); return false;
}
    */

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

  update_view(c);
  return true;
}

bool submit(context *c, uint32_t frame_index, uint32_t image_index) {
  logger l{c->log_level};
  const VkFence f = c->per_frame[frame_index].presentation_done.handle;
  const VkCommandBuffer rb = c->per_frame[frame_index].graphics_buffer;
  VkSubmitInfo sinfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  sinfo.waitSemaphoreCount = 1;
  sinfo.pWaitSemaphores = &c->per_frame[frame_index].image_available.handle;
  sinfo.pWaitDstStageMask = wait_stages;
  sinfo.commandBufferCount = 1;
  sinfo.pCommandBuffers = &rb;
  sinfo.signalSemaphoreCount = 1;
  sinfo.pSignalSemaphores = &c->per_frame[frame_index].rendering_done.handle;

  if (vkQueueSubmit(c->graphics_queue, 1, &sinfo, f) != VK_SUCCESS) {
    l.loge("Failed to submit commands to graphics queue\n");
    return false;
  }

  return true;
}

void present(context *c, uint32_t frame_index, uint32_t image_index) {
  VkPresentInfoKHR pinfo{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  pinfo.waitSemaphoreCount = 1;
  pinfo.pWaitSemaphores = &c->per_frame[frame_index].rendering_done.handle;
  pinfo.pSwapchains = &c->swapchain.handle;
  pinfo.swapchainCount = 1;
  pinfo.pImageIndices = &image_index;
  vkQueuePresentKHR(c->presentation_queue, &pinfo);
}

bool record(context *c, uint32_t frame_index, uint32_t image_index) {
  logger l{c->log_level};
  const VkCommandBuffer rb = c->per_frame[frame_index].graphics_buffer;
  vkResetCommandBuffer(rb, 0);

  VkCommandBufferBeginInfo cb_begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  if (vkBeginCommandBuffer(rb, &cb_begin_info) != VK_SUCCESS) {
    l.loge("Failed to begin command buffer\n");
    return false;
  }

  VkRenderPassBeginInfo rp_begin_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  rp_begin_info.framebuffer = c->framebuffers[image_index].handle;
  rp_begin_info.renderPass = c->render_pass.handle;
  rp_begin_info.renderArea.extent = {c->window_width, c->window_height};
  rp_begin_info.renderArea.offset = {0, 0};

  VkClearValue clear_values[2];
  clear_values[1].depthStencil = {.depth = 1.f, .stencil = 0};
  clear_values[0].color = {0.f, 0.f, 0.f, 1.f};
  rp_begin_info.pClearValues = clear_values;
  rp_begin_info.clearValueCount =
      sizeof(clear_values) / sizeof(clear_values[0]);

  vkCmdBeginRenderPass(rb, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(rb, VK_PIPELINE_BIND_POINT_GRAPHICS, c->pipeline.handle);
	vkCmdBindDescriptorSets(rb, VK_PIPELINE_BIND_POINT_GRAPHICS,
			c->layout.handle, 0, 1, &c->per_frame[c->frame_index].descriptor_set,
			0, 0);

  VkDeviceSize offset{0};
  vkCmdBindVertexBuffers(rb, 0, 1, &c->vertex_buffer.handle, &offset);
	vkCmdBindIndexBuffer(rb, c->index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

        vkCmdSetViewport(rb, 0, 1, &c->viewport);
        vkCmdSetScissor(rb, 0, 1, &c->scissor);
        vkCmdDraw(rb, c->vertices.size(), 1, 0, 0);
        // vkCmdDrawIndexed(rb, c->indices.size(), 1, 0, 0, 0);
        vkCmdEndRenderPass(rb);

        if (vkEndCommandBuffer(rb) != VK_SUCCESS) {
          l.loge("Failed to end command buffer\n");
          return false;
  }

  return true;
}

void initialize_dynamic_state(context *c) {
  c->viewport.width = (float)c->window_width;
  c->viewport.height = (float)c->window_height;
  c->viewport.minDepth = 0.0f;
  c->viewport.maxDepth = 1.0f;
  c->viewport.x = 0.0f;
  c->viewport.y = 0.0f;

  c->scissor.extent = {c->window_width, c->window_height};
  c->scissor.offset = {0, 0};
}

bool render(context *c) {
  const VkSemaphore ia = c->per_frame[c->frame_index].image_available.handle;
  const VkFence f = c->per_frame[c->frame_index].presentation_done.handle;
  const VkSwapchainKHR chain = c->swapchain.handle;
  const VkDevice dev = c->device.handle;
  logger l{c->log_level};

  auto r = vkWaitForFences(dev, 1, &f, VK_TRUE, 0);
  if (r == VK_TIMEOUT)
    return true;

  uint32_t image_index{};
  r = vkAcquireNextImageKHR(dev, chain, 0, ia, 0, &image_index);
  if (r != VK_SUCCESS) {
    if (r == VK_NOT_READY)
      return true;

    l.loge("Failed to acquire an image from the swapchain\n");
    return false;
  }

  vkResetFences(dev, 1, &f);

  if (!record(c, c->frame_index, image_index))
    return false;

  if (!submit(c, c->frame_index, image_index))
    return false;

  present(c, c->frame_index, image_index);

  c->frame_index = (c->frame_index + 1) % c->concurrent_frames;
  return true;
}

bool initialize(context *c, const fs::path bin,
                const std::vector<std::string> cli) {
  fs::current_path(fs::absolute(bin.parent_path()));
  initialize_dynamic_state(c);
  logger l{logger::err};

  if (!parse_cli(c, &cli)) {
    std::cerr << "(ERROR): The command line input is not valid\n";
    return false;
  }
  l = logger{c->log_level};

  if (!initialize_glfw(c)) {
    l.loge("GLFW initialization failed\n");
    return false;
  }

  if (!create_instance(c)) {
    l.loge("Instance creation failed\n");
    return false;
  }

  if (!select_physical(c)) {
    l.loge("Device selection failed\n");
    return false;
  }

  if (!create_device(c)) {
    l.loge("Device creation failed\n");
    return false;
  }

  if (!create_memory_allocator(c)) {
    l.loge("VMA allocator creation failed\n");
    return false;
  }

  if (!create_window(c)) {
    l.loge("Window creation failed\n");
    return false;
  }

  if (!create_surface(c)) {
    l.loge("Window surface creation failed\n");
    return false;
  }

  if (!create_swapchain(c)) {
    l.loge("Swapchain creation failed\n");
    return false;
  }

  if (!create_image_views(c)) {
    l.loge("Image view creation failed\n");
    return false;
  }

  if (!create_depth_images(c)) {
    l.loge("Depth image creation failed\n");
    return false;
  }

  if (!create_render_pass(c)) {
    l.loge("Render pass creation failed\n");
    return false;
  }

  if (!create_framebuffers(c)) {
    l.loge("Framebuffer creation failed\n");
    return false;
  }

  if (!create_descriptor_pool(c)) {
    l.loge("Descriptor creation failed\n");
    return false;
  }

 	if (!create_pipeline_layout(c)) {
    l.loge("Failed to create pipeline layout\n");
    return false;
  }

  if (!create_pipeline(c)) {
    l.loge("Pipeline creation failed\n");
    return false;
  }

  if (!create_semaphores(c)) {
    l.loge("Semaphore creation failed\n");
    return false;
  }

  if (!create_buffers(c)) {
    l.loge("Command pool and buffer creation failed\n");
    return false;
  }

  if (!configure_sigil_vertices(c)) {
    l.loge("Failed to configure sigil\n");
    return false;
  }

  return true;
}

bool read_matrix(const std::string &path, std::vector<std::vector<vtype>> *d) {
  if (!d)
    return false;

  std::ifstream stream{path};
  if (!stream.is_open())
    return false;

  std::vector<vtype> linear{};
  vtype entry{};
  while (true) {
    stream >> entry;
    if (stream.eof() || stream.bad())
      break;
    linear.push_back(entry);
  }

  if (stream.bad())
    return false;

  const double root = std::sqrt(linear.size());
  const std::size_t trunc = root;
  if (trunc * trunc != linear.size())
    return false; // matrix is not square

  d->resize(trunc);
  for (std::size_t i = 0; i < d->size(); ++i)
    (*d)[i].resize(trunc);

  std::size_t row{}, col{};
  for (std::size_t i = 0; i < linear.size(); ++i) {
    (*d)[row][col++] = linear[i];
    if (col == trunc) {
      col = 0;
      ++row;
    }
  }
  return true;
}

bool configure_sigil_vertices(context *c) {
  logger l{c->log_level};
  std::vector<std::vector<vtype>> data{};
  if (!read_matrix(c->matrix_source_file, &data)) {
    l.loge("Failed to read matrix from source file\n");
    return false;
  }

  c->vertices = normalize_matrix(data);
  c->update_buffers = true;
  return true;
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

  vb.usage =
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (vmaCreateBuffer(a0, &vb, &aci, &ibuf, &ialloc, &iai) != VK_SUCCESS) {
    l.loge("Failed to create vk buffer using VMA\n");
    return false;
  }
  c->index_buffer = raii::resource<adapter::vma_buffer>{a0, ialloc, ibuf};

  return true;
}

bool create_framebuffers(context *c) {
  c->framebuffers.resize(c->images.size());
  const VkDevice dev = c->device.handle;
  logger l{c->log_level};

  VkFramebufferCreateInfo info{.sType =
                                   VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  info.renderPass = c->render_pass.handle;
  info.width = c->window_width;
  info.height = c->window_height;
  info.layers = 1;

  for (std::size_t i = 0; i < c->images.size(); ++i) {
    VkImageView attachments[] = {c->image_views[i].handle,
                                 c->depth_views[i].handle};

    info.attachmentCount = sizeof(attachments) / sizeof(attachments[0]);
    info.pAttachments = attachments;
    VkFramebuffer handle{};

    auto r = vkCreateFramebuffer(dev, &info, nullptr, &handle);
    if (r != VK_SUCCESS) {
      l.loge("Failed to create framebuffer\n");
      return false;
    }
    c->framebuffers[i] = raii::resource<adapter::vk_framebuffer>{dev, handle};
  }

  return true;
}

bool conf_shader(const VkDevice dev, std::vector<uint32_t> *src,
                 VkPipelineShaderStageCreateInfo *info, auto *mod,
                 VkShaderStageFlagBits type) {

  VkShaderModuleCreateInfo inf{};
  inf.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  inf.codeSize = src->size() * sizeof(uint32_t);
  inf.pCode = src->data();

  VkShaderModule handle{};
  auto r = vkCreateShaderModule(dev, &inf, nullptr, &handle);
  if (r != VK_SUCCESS)
    return false;
  *mod = raii::resource<adapter::vk_shader_module>{dev, handle};

  info->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info->stage = type;
  info->module = mod->handle;
  info->pName = "main";

  return true;
}

bool conf_shaders(const VkDevice dev, VkPipelineShaderStageCreateInfo *info,
                  auto *mod, std::size_t log_level) {
  logger l{log_level};

  *info = {};
  *(info + 1) = {};

  std::vector<uint32_t> vert_src{};
  auto r = shader::read_spirv("./vertex_shader.spv", &vert_src);
  if (r != common::result::success) {
    l.loge("Failed to read shader source file\n");
    return false;
  }

  std::vector<uint32_t> frag_src{};
  r = shader::read_spirv("./fragment_shader.spv", &frag_src);
  if (r != common::result::success) {
    l.loge("Failed to read shader source file\n");
    return false;
  }

  if (!conf_shader(dev, &vert_src, info, mod, VK_SHADER_STAGE_VERTEX_BIT)) {
    l.loge("Failed to create vertex shader module\n");
    return false;
  }

  if (!conf_shader(dev, &frag_src, (info + 1), (mod + 1),
                   VK_SHADER_STAGE_FRAGMENT_BIT)) {
    l.loge("Failed to create fragment shader module\n");
    return false;
  }

  return true;
}

bool conf_dynamic_state(VkPipelineDynamicStateCreateInfo *info,
                        std::vector<VkDynamicState> *states,
                        VkPipelineViewportStateCreateInfo *vp,
                        VkViewport *viewport, VkRect2D *scissor, uint32_t width,
                        uint32_t height) {

  states->resize(2);
  (*states)[0] = VK_DYNAMIC_STATE_VIEWPORT;
  (*states)[1] = VK_DYNAMIC_STATE_SCISSOR;

  info->sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  info->dynamicStateCount = static_cast<uint32_t>(states->size());
  info->pDynamicStates = states->data();

  viewport->x = 0.0f;
  viewport->y = 0.0f;
  viewport->width = (float)width;
  viewport->height = (float)height;
  viewport->minDepth = 0.0f;
  viewport->maxDepth = 1.0f;

  scissor->offset = {0, 0};
  scissor->extent = {width, height};

  vp->sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  vp->viewportCount = 1;
  vp->scissorCount = 1;
  vp->pViewports = viewport;
  vp->pScissors = scissor;
  return true;
}

bool conf_assembly(VkPipelineInputAssemblyStateCreateInfo *a,
                   VkPipelineRasterizationStateCreateInfo *r,
                   VkPipelineMultisampleStateCreateInfo *m) {
  a->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  a->topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  a->primitiveRestartEnable = VK_FALSE;

  r->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  r->depthClampEnable = VK_TRUE;
  r->rasterizerDiscardEnable = VK_FALSE;
  r->polygonMode = VK_POLYGON_MODE_FILL;
  r->lineWidth = 1.0f;
  r->cullMode = VK_CULL_MODE_NONE;
  r->depthBiasEnable = VK_FALSE;

  m->sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  m->sampleShadingEnable = VK_FALSE;
  m->rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  m->minSampleShading = 1.0f;
  m->pSampleMask = nullptr;
  m->alphaToCoverageEnable = VK_FALSE;
  m->alphaToOneEnable = VK_FALSE;
  return true;
}

bool conf_color(VkPipelineColorBlendAttachmentState *state,
                VkPipelineColorBlendStateCreateInfo *blend) {
  state->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  state->blendEnable = VK_TRUE;
  state->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  state->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  state->colorBlendOp = VK_BLEND_OP_ADD;
  state->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  state->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  state->alphaBlendOp = VK_BLEND_OP_ADD;

  blend->sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend->logicOpEnable = VK_FALSE;
  blend->logicOp = VK_LOGIC_OP_COPY; // Optional
  blend->attachmentCount = 1;
  blend->pAttachments = state;
  blend->blendConstants[0] = 0.0f; // Optional
  blend->blendConstants[1] = 0.0f; // Optional
  blend->blendConstants[2] = 0.0f; // Optional
  blend->blendConstants[3] = 0.0f; // Optional
  return true;
}

bool create_descriptor_pool(context* c) {
  logger l{c->log_level};
	VkDescriptorPool handle{};
	VkDescriptorPoolCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};

	VkDescriptorPoolSize size{};
	size.descriptorCount = c->concurrent_frames;
	size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	info.pPoolSizes = &size;
	info.poolSizeCount = 1;
	info.maxSets = c->concurrent_frames;

	if (vkCreateDescriptorPool(c->device.handle,
				&info, nullptr, &handle) != VK_SUCCESS) {
		l.loge("Failed to create descriptor pool\n");
		return false;
	}
	c->desc_pool = raii::resource<adapter::vk_descriptor_pool>{c->device.handle,
		handle};

	return true;
}

bool create_pipeline_layout(context* c) {
  logger l{c->log_level};
	const VkDevice dev = c->device.handle;
	VkDescriptorSetLayoutBinding bi{};
	bi.binding = 0;
	bi.descriptorCount = 1;
	bi.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bi.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo li{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	li.bindingCount = 1;
	li.pBindings = &bi;

	VkDescriptorSetLayout layout{};
	if (vkCreateDescriptorSetLayout(c->device.handle, &li, nullptr, &layout)
			!= VK_SUCCESS) {
		l.loge("Failed to create descriptor set layout\n");
		return false;
	}
	c->desc_layout = raii::resource<adapter::vk_descriptor_set_layout>{
		c->device.handle, layout
	};

  VkPipelineLayoutCreateInfo info{};
	VkPipelineLayout handle{};

	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount = 1;
	info.pSetLayouts = &c->desc_layout.handle;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(dev, &info, nullptr, &handle) != VK_SUCCESS)
		return false;
	c->layout = raii::resource<adapter::vk_pipeline_layout>{dev, handle};

	std::vector<VkDescriptorSetLayout> layouts{};
	for (std::size_t i = 0; i < c->concurrent_frames; ++i)
		layouts.push_back(layout);

	VkDescriptorSetAllocateInfo ai{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	ai.descriptorPool = c->desc_pool.handle;
	ai.descriptorSetCount = layouts.size();
	ai.pSetLayouts = layouts.data();
	std::array<VkDescriptorSet, context::concurrent_frames> descs{};

	if (vkAllocateDescriptorSets(dev, &ai, descs.data()) != VK_SUCCESS) {
		l.loge("Failed to allocate descriptor sets\n");
		return false;
	}	
	for (std::size_t i = 0; i < c->concurrent_frames; ++i)
		c->per_frame[i].descriptor_set = descs[i];

	return true;
}

bool conf_vertex_input_info(VkPipelineVertexInputStateCreateInfo *info,
                            VkVertexInputBindingDescription *binding_desc,
                            const vertex::attr_desc_t *attrib_desc) {

  binding_desc->binding = 0;
  binding_desc->stride = sizeof(vertex);
  binding_desc->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  info->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  info->vertexBindingDescriptionCount = 1;
  info->pVertexBindingDescriptions = binding_desc;
  info->vertexAttributeDescriptionCount = attrib_desc->size();
  info->pVertexAttributeDescriptions = attrib_desc->data();
  return true;
}

bool create_pipeline(context *c) {
  logger l{c->log_level};

  raii::resource<adapter::vk_shader_module> modules[2];
  VkPipelineShaderStageCreateInfo shader_stages[2];
  if (!conf_shaders(c->device.handle, shader_stages, modules, c->log_level))
    return false;

  VkPipelineViewportStateCreateInfo viewport_state{};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  std::vector<VkDynamicState> dynamic_states{};
  VkViewport viewport{};
  VkRect2D scissor{};
  conf_dynamic_state(&dynamic_state, &dynamic_states, &viewport_state,
                     &viewport, &scissor, c->window_width, c->window_height);

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  auto attrib_desc = vertex::attribute_description();
  VkVertexInputBindingDescription vbd{};
  conf_vertex_input_info(&vertex_input, &vbd, &attrib_desc);

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  VkPipelineMultisampleStateCreateInfo multisampling{};
  conf_assembly(&input_assembly, &rasterizer, &multisampling);

  VkPipelineColorBlendAttachmentState color_attachment{};
  VkPipelineColorBlendStateCreateInfo color_blending{};
  conf_color(&color_attachment, &color_blending);

  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable = VK_TRUE;
  depth.depthWriteEnable = VK_TRUE;
  depth.depthCompareOp = VK_COMPARE_OP_LESS;

  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.stageCount = sizeof(shader_stages) / sizeof(shader_stages[0]);
  info.pStages = shader_stages;
  info.pRasterizationState = &rasterizer;
  info.pVertexInputState = &vertex_input;
  info.renderPass = c->render_pass.handle;
  info.subpass = 0;
  info.layout = c->layout.handle;
  info.pViewportState = &viewport_state;
  info.pInputAssemblyState = &input_assembly;
  info.pMultisampleState = &multisampling;
  info.pColorBlendState = &color_blending;
  info.pDynamicState = &dynamic_state;
  info.pDepthStencilState = &depth;

  VkPipeline handle{};
  auto r = vkCreateGraphicsPipelines(c->device.handle, 0, 1, &info, 0, &handle);
  if (r != VK_SUCCESS) {
    l.loge("Failed to create pipeline with code: " + std::to_string(r), "\n");
    return false;
  }

  c->pipeline = raii::resource<adapter::vk_pipeline>{c->device.handle, handle};
  return true;
}

bool create_render_pass(context *c) {
  VkAttachmentDescription attachment{};
  attachment.format = c->surface_format.format;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentDescription depth{};
  depth.format = c->depth_format;
  depth.samples = VK_SAMPLE_COUNT_1_BIT;
  depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference attachment_ref{};
  attachment_ref.attachment = 0;
  attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 1;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &attachment_ref;
  subpass.pDepthStencilAttachment = &depth_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;

  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  VkAttachmentDescription descs[] = {attachment, depth};
  info.attachmentCount = sizeof(descs) / sizeof(descs[0]);
  info.pAttachments = descs;
  info.subpassCount = 1;
  info.pSubpasses = &subpass;
  info.dependencyCount = 1;
  info.pDependencies = &dependency;

  const auto device = c->device.handle;
  VkRenderPass handle{};
  if (vkCreateRenderPass(device, &info, nullptr, &handle) != VK_SUCCESS)
    return false;

  c->render_pass = raii::resource<adapter::vk_render_pass>{device, handle};
  return true;
}

bool create_image_views(context *c) {
  logger l{c->log_level};

  VkImageViewCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = c->surface_format.format;
  info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.levelCount = 1;

  c->image_views.resize(c->images.size());
  std::vector<VkImageView> views{};
  views.resize(c->images.size());
  const VkDevice dev = c->device.handle;

  for (std::size_t i = 0; i < c->images.size(); ++i) {
    info.image = c->images[i];
    auto r = vkCreateImageView(dev, &info, nullptr, &views[i]);

    if (r != VK_SUCCESS) {
      l.loge("Failed to create image view\n");
      return false;
    }

    c->image_views[i] = raii::resource<adapter::vk_image_view>{dev, views[i]};
  }

  return true;
}

uint32_t select_image_count(context *c) {
  const auto max_img_count = c->surface_capabilities.capabilities.maxImageCount;
  auto image_count = c->surface_capabilities.capabilities.minImageCount + 1;
  if (max_img_count && image_count > max_img_count)
    image_count = max_img_count;

  return image_count;
}

bool select_surface_format(context *c) {
  logger l{c->log_level};

  if (!c->surface_capabilities.surface_formats.size()) {
    l.loge("No surface formats available\n");
    return false;
  }

  c->surface_format = c->surface_capabilities.surface_formats.front();
  for (const auto &f : c->surface_capabilities.surface_formats)
    if (f.format == VK_FORMAT_B8G8R8A8_UNORM) {
      c->surface_format = f;
      break;
    }

  std::string f = std::to_string(c->surface_format.format);
  l.logi("Selected surface format: " + f + "\n");
  return true;
}

bool create_swapchain(context *c) {
  auto image_count = select_image_count(c);
  logger l{c->log_level};

  if (!select_surface_format(c))
    return false;
  auto format = c->surface_format;

  VkExtent2D size{.width = c->window_width, .height = c->window_height};
  l.logi("Selected image size: " + std::to_string(size.width) + "x" +
             std::to_string(size.height),
         "\n");

  int usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if (c->surface_capabilities.capabilities.supportedUsageFlags &
      VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  else {
    l.loge("Swapchain images cannot be used as a transfer destination\n");
    return false;
  }

  auto transform = c->surface_capabilities.capabilities.currentTransform;
  if (c->surface_capabilities.capabilities.supportedTransforms &
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

  auto present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (std::size_t i = 0; i < c->surface_capabilities.present_modes.size(); ++i)
    if (c->surface_capabilities.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
      present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

  l.logi("Selected present mode: " + std::to_string(present_mode), "\n");

  const VkDevice device = c->device.handle;
  VkSwapchainCreateInfoKHR info{};
  VkSwapchainKHR handle{};

  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.surface = c->surface.handle;
  info.minImageCount = image_count;
  info.imageFormat = format.format;
  info.imageColorSpace = format.colorSpace;
  info.imageExtent = size;
  info.imageArrayLayers = 1;
  info.imageUsage = usage;
  info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  info.preTransform = transform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = present_mode;
  info.clipped = VK_TRUE;
  info.oldSwapchain = c->swapchain.handle;

  auto r = vkCreateSwapchainKHR(device, &info, 0, &handle);
  if (r != VK_SUCCESS) {
    l.loge("Failed to create swapchain with code: " + std::to_string(r));
    return false;
  }
  c->swapchain = raii::resource<adapter::vk_swapchain>{device, handle};

  uint32_t count{};
  r = vkGetSwapchainImagesKHR(device, handle, &count, nullptr);
  c->images.resize(count);
  r = vkGetSwapchainImagesKHR(device, handle, &count, c->images.data());

  if (r != VK_SUCCESS) {
    l.loge("Failed to get swapchain images with code: " + std::to_string(r));
    return false;
  }
  return true;
}

bool create_window(context *c) {
  logger l{c->log_level};

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow *handle{};
  handle = glfwCreateWindow(c->window_width, c->window_height, "Sigil", 0, 0);

  if (!handle) {
    l.loge("Failed to create GLFW window\n");
    return false;
  }

  glfwSetKeyCallback(handle, [](GLFWwindow *w, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
      glfwSetWindowShouldClose(w, GLFW_TRUE);
  });

  c->window = raii::resource<adapter::glfw_window>{handle};
  return true;
}

bool create_surface(context *c) {
  logger l{c->log_level};

  VkSurfaceKHR handle{};
  auto r = glfwCreateWindowSurface(c->instance.handle, c->window.handle,
                                   nullptr, &handle);

  if (r != VK_SUCCESS) {
    l.loge("Failed to create window surface with code: " + std::to_string(r));
    return false;
  }
  c->surface = raii::resource<adapter::vk_surface>{c->instance.handle, handle};

  specs::vk_surface sspecs{};
  auto res = query::surface_specs(&sspecs, c->selected_device, handle);
  if (res != common::result::success) {
    l.loge("Failed to query surface specs");
    return false;
  }
  c->surface_capabilities = std::move(sspecs);
  return true;
}

bool create_semaphore(VkSemaphore *handle, const VkDevice device) {
  logger l{logger::err};

  VkSemaphoreCreateInfo info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  auto r = vkCreateSemaphore(device, &info, 0, handle);

  if (r != VK_SUCCESS) {
    l.loge("Failed to create image available sempaphore with code: " +
           std::to_string(r));
    return false;
  }

  return true;
  ;
}

bool create_semaphores(context *c) {
  VkFenceCreateInfo finf{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  finf.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  const VkDevice device = c->device.handle;
  logger l{c->log_level};
  VkSemaphore handle{};
  VkFence fence{};

  for (std::size_t i = 0; i < c->concurrent_frames; ++i) {
    if (!create_semaphore(&handle, device))
      return false;
    c->per_frame[i].image_available =
        raii::resource<adapter::vk_semaphore>{device, handle};

    if (!create_semaphore(&handle, device))
      return false;
    c->per_frame[i].rendering_done =
        raii::resource<adapter::vk_semaphore>{device, handle};

    auto r = vkCreateFence(device, &finf, nullptr, &fence);
    if (r != VK_SUCCESS) {
      l.loge("Failed to create fence with code: " + std::to_string(r));
      return false;
    }
    c->per_frame[i].presentation_done =
        raii::resource<adapter::vk_fence>{device, fence};
  }

  return true;
}

bool create_buffers(context *c) {
  logger l{c->log_level};

  VkCommandPool handle{VK_NULL_HANDLE};
  VkCommandPoolCreateInfo info{.sType =
                                   VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
               VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  const VkDevice device = c->device.handle;

  info.queueFamilyIndex = c->presentation_queue_family_index;
  auto r = vkCreateCommandPool(device, &info, 0, &handle);
  if (r != VK_SUCCESS) {
    auto code = std::to_string(r);
    l.loge("Failed to create presentation command pool with code: " + code);
    return false;
  }
  c->presentation_command_pool =
      raii::resource<adapter::vk_command_pool>{device, handle};

  info.queueFamilyIndex = c->graphics_queue_family_index;
  VkCommandPool graphics_pool{VK_NULL_HANDLE};
  r = vkCreateCommandPool(device, &info, 0, &handle);
  if (r != VK_SUCCESS) {
    auto code = std::to_string(r);
    l.loge("Failed to create graphics command pool with code: " + code);
    return false;
  }
  c->graphics_command_pool =
      raii::resource<adapter::vk_command_pool>{device, handle};

  std::vector<VkCommandBuffer> pbuffers(c->concurrent_frames);
  VkCommandBufferAllocateInfo cbinfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  cbinfo.commandBufferCount = c->concurrent_frames;
  cbinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  cbinfo.commandPool = c->presentation_command_pool.handle;
  r = vkAllocateCommandBuffers(device, &cbinfo, pbuffers.data());
  if (r != VK_SUCCESS) {
    auto code = std::to_string(r);
    l.loge("Failed to allocate command buffers with code: " + code);
    return false;
  }
  for (std::size_t i = 0; i < c->concurrent_frames; ++i)
    c->per_frame[i].presentation_buffer = pbuffers[i];

  std::vector<VkCommandBuffer> gbuffers(c->concurrent_frames);
  cbinfo.commandPool = c->graphics_command_pool.handle;
  r = vkAllocateCommandBuffers(device, &cbinfo, gbuffers.data());
  if (r != VK_SUCCESS) {
    auto code = std::to_string(r);
    l.loge("Failed to allocate command buffers with code: " + code);
    return false;
  }
  for (std::size_t i = 0; i < c->concurrent_frames; ++i)
    c->per_frame[i].graphics_buffer = gbuffers[i];

  auto vb = VkBufferCreateInfo{};
  auto a0 = c->allocator.handle;
  vb.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  vb.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vb.size = sizeof(transformation);
  vb.usage =
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  vb.usage =
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  VmaAllocationCreateInfo aci{};
  aci.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
  aci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

	for (std::size_t i = 0; i < c->concurrent_frames; ++i) {
		VmaAllocationInfo ai{};
		VmaAllocation alloc{};
		VkBuffer handle{};
		if (vmaCreateBuffer(a0, &vb, &aci, &handle, &alloc, &ai) != VK_SUCCESS) {
			l.loge("Failed to create descriptor buffer using VMA\n");
			return false;
		}
		c->per_frame[i].desc_buffer = raii::resource<adapter::vma_buffer>{
			a0, alloc, handle};
	}
  return true;
}

void assign_queue_family_indices(context *c) {
  logger l{c->log_level};
  const auto &qf = c->device_capabilities.queue_families;
  for (std::size_t i = 0; i < qf.size(); ++i) {
    if (qf[i].presentation_support) {
      c->presentation_queue_family_index = i;
      break;
    }
  }

  for (std::size_t i = 0; i < qf.size(); ++i) {
    if (qf[i].properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      c->graphics_queue_family_index = i;
      break;
    }
  }

  for (std::size_t i = 0; i < qf.size(); ++i) {
    if (qf[i].presentation_support &&
        qf[i].properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {

      c->presentation_queue_family_index = i;
      c->graphics_queue_family_index = i;
      l.logi("Using the same queue family for presentation and graphics: " +
                 std::to_string(i),
             "\n");
      break;
    }
  }
}

bool create_memory_allocator(context *c) {
  VmaAllocatorCreateInfo info{};
  info.vulkanApiVersion = c->app_info.apiVersion;
  info.physicalDevice = c->selected_device;
  info.instance = c->instance.handle;
  info.device = c->device.handle;

  VmaAllocator handle{};
  if (vmaCreateAllocator(&info, &handle) != VK_SUCCESS)
    return false;

  c->allocator = raii::resource<adapter::vk_memory_allocator>{handle};
  return true;
}

bool create_device(context *c) {
  logger l{c->log_level};

  assign_queue_family_indices(c);
  std::vector<float> prio{1.f};

  VkDeviceQueueCreateInfo qinfos[] = {
      {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
       .queueFamilyIndex = c->presentation_queue_family_index,
       .queueCount = static_cast<uint32_t>(prio.size()),
       .pQueuePriorities = prio.data()},

      {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
       .queueFamilyIndex = c->graphics_queue_family_index,
       .queueCount = static_cast<uint32_t>(prio.size()),
       .pQueuePriorities = prio.data()}};

  VkDeviceCreateInfo info{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};

  if (c->presentation_queue_family_index != c->graphics_queue_family_index) {
    info.pQueueCreateInfos = qinfos;
    info.queueCreateInfoCount = sizeof(qinfos) / sizeof(qinfos[0]);
  } else {
    info.pQueueCreateInfos = &qinfos[0];
    info.queueCreateInfoCount = 1;
  }

  static constexpr const char *swpx = "VK_KHR_swapchain";
  info.ppEnabledExtensionNames = &swpx;
  info.enabledExtensionCount = 1;
  VkPhysicalDeviceFeatures features{};
  features.depthClamp = VK_TRUE;
  info.pEnabledFeatures = &features;

  VkDevice handle{VK_NULL_HANDLE};
  auto r = vkCreateDevice(c->selected_device, &info, 0, &handle);
  if (r != VK_SUCCESS) {
    auto code = std::to_string(r);
    l.loge("Failed to create logical device with error code: ", code, "\n");
    return false;
  }
  c->device = raii::resource<adapter::vk_device>{handle};

  vkGetDeviceQueue(handle, c->presentation_queue_family_index, 0,
                   &c->presentation_queue);
  vkGetDeviceQueue(handle, c->graphics_queue_family_index, 0,
                   &c->graphics_queue);

  return true;
}

bool select_physical(context *c) {
  logger l{c->log_level};

  std::vector<VkPhysicalDevice> pdevs{};
  auto r = query::available_devices(&pdevs, c->instance.handle);
  if (r != common::result::success) {
    l.loge("Querying available physical devices failed\n");
    return false;
  }

  if (!pdevs.size()) {
    l.loge("No physical devices available\n");
    return false;
  }

  struct dspec {
    VkPhysicalDevice handle{};
    specs::vk_device spec{};
  };
  struct entry {
    std::size_t score{};
    dspec device{};
  };
  std::vector<entry> entries{};

  for (auto dev : pdevs) {
    specs::vk_device dev_specs{};
    std::size_t score{1};

    auto r = query::device_specs(&dev_specs, c->instance.handle, dev);
    if (r != common::result::success) {
      l.loge("Querying physical device failed\n");
      return false;
    }

    bool is_presentation_support{false};
    for (const auto &qf : dev_specs.queue_families)
      if (qf.presentation_support) {
        is_presentation_support = true;
        break;
      }

    if (!is_available(&dev_specs.extensions, "VK_KHR_swapchain"))
      score = 0;

    score *= dev_specs.properties.limits.maxImageDimension2D;
    entries.push_back({score, {dev, std::move(dev_specs)}});
  }

  if (entries.size()) {
    std::sort(entries.begin(), entries.end(),
              [](const auto &a, const auto &b) { return a.score > b.score; });
    c->device_capabilities = std::move(entries.front().device.spec);
    c->selected_device = entries.front().device.handle;
  }

  if (!entries.size()) {
    l.loge("Failed to acquire a physical device\n");
    return false;
  }

  auto score = std::to_string(entries.front().score);
  auto name = std::string{c->device_capabilities.properties.deviceName};
  l.logi("Selected device: ", name, ", score: ", score, "\n");
  return true;
}

bool create_instance(context *c) {
  logger l{c->log_level};

  auto r = query::instance_specs(&c->instance_capabilities);
  if (r != common::result::success)
    l.loge("Failed to query instance specs\n");

  constexpr uint32_t STABLE = 0;
  c->app_info.apiVersion = VK_MAKE_API_VERSION(STABLE, 1, 3, 0);
  c->app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  c->app_info.pApplicationName = "Sigil";

  VkInstanceCreateInfo info{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  info.pApplicationInfo = &c->app_info;

  uint32_t xcount{};
  glfwGetRequiredInstanceExtensions(&xcount);
  auto data = glfwGetRequiredInstanceExtensions(&xcount);

  l.logi("GLFW requested ", std::to_string(xcount), " instance extensions:\n");
  for (std::size_t i = 0; i < xcount; ++i)
    l.logs("\t", data[i], "\n");

  for (std::size_t i = 0; i < xcount; ++i)
    if (!is_available(&c->instance_capabilities.extensions, data[i])) {
      auto ext = std::string{data[i]};
      l.loge("The required extension is not available: ", ext);
      return false;
    }

  if (xcount) {
    info.enabledExtensionCount = xcount;
    info.ppEnabledExtensionNames = data;
  }

  if (c->debug) {
    static constexpr const char *debug_layer = "VK_LAYER_KHRONOS_validation";
    if (!is_available(&c->instance_capabilities.layers, debug_layer)) {
      l.loge("The KHRONOS validation layer is not available!");
      return false;
    }
    info.ppEnabledLayerNames = &debug_layer;
    info.enabledLayerCount = 1;
  }

  VkInstance handle{VK_NULL_HANDLE};
  if (auto r = vkCreateInstance(&info, 0, &handle); r != VK_SUCCESS) {
    auto code = std::to_string(static_cast<uint32_t>(r));
    l.loge("Instance creation failed with error code: ", code);
    return false;
  }

  c->instance = raii::resource<adapter::vk_instance>{handle};
  return true;
}

bool initialize_glfw(context *c) {
  if (glfwInit() == GLFW_TRUE) {
    c->glfw_guard.cleanup(true);
    return true;
  }
  return false;
}

bool parse_cli(context *c, const std::vector<std::string> *input) {
  if (!input->size())
    return true;

  using cfg::add_rule;
  using cfg::bind;

  cfg::lexer_table_t tbl{};
  cfg::add_entry(&tbl, cfg::token_type::flag, "verbose-flag", "-v|--verbose");
  cfg::add_entry(&tbl, cfg::token_type::flag, "help-flag", "--help");
  cfg::add_entry(&tbl, cfg::token_type::flag, "debug-flag", "-d|--debug");
  cfg::add_entry(&tbl, cfg::token_type::option, "file-option", "-f|--file");
  cfg::add_entry(&tbl, cfg::token_type::option, "width-option", "-w|--width");
  cfg::add_entry(&tbl, cfg::token_type::option, "height-option", "-h|--height");
  cfg::add_entry(&tbl, cfg::token_type::free, "size-tok", "[1-9]\\d{2,3}");
  cfg::add_entry(&tbl, cfg::token_type::free, "string-tok", ".+");

  cfg::action_map_t m{};
  cfg::grammar_t g{};
  logger l{};

  unsigned verbose_count{}, debug_count{}, width_count{}, height_count{},
      file_count{};

  const cfg::action_t help = [c](auto *, auto *, auto *) { c->help = true; };
  const cfg::action_t v = [c, &verbose_count](auto *, auto *, auto *) {
    c->log_level = logger::inf | logger::wrn | logger::err;
    ++verbose_count;
  };

  const cfg::action_t d = [c, &debug_count](auto *, auto *, auto *) {
    c->debug = true;
    ++debug_count;
  };

  const cfg::action_t w = [c, &width_count](auto *, auto *, auto *s) {
    c->window_width = std::stoull(s->value);
    ++width_count;
  };

  const cfg::action_t h = [c, &height_count](auto *, auto *, auto *s) {
    c->window_height = std::stoull(s->value);
    ++height_count;
  };

  const cfg::action_t f = [c, &file_count](auto *, auto *, auto *s) {
    c->matrix_source_file = s->value;
    ++file_count;
  };

  bind(&m, add_rule(&g, "start", "help-flag"), help);
  bind(&m, add_rule(&g, "start", "verbose-flag"), v);
  bind(&m, add_rule(&g, "start", "debug-flag"), d);
  bind(&m, add_rule(&g, "start", "file-option#0", "string-tok#0"), f);
  bind(&m, add_rule(&g, "start", "width-option#0", "size-tok#0"), w);
  bind(&m, add_rule(&g, "start", "height-option#0", "size-tok#0"), h);

  bind(&m, add_rule(&g, "arg_list", "verbose-flag"), v);
  bind(&m, add_rule(&g, "arg_list", "debug-flag"), d);
  bind(&m, add_rule(&g, "arg_list", "width-option#0", "size-tok#0"), w);
  bind(&m, add_rule(&g, "arg_list", "height-option#0", "size-tok#0"), h);
  bind(&m, add_rule(&g, "arg_list", "file-option#0", "string-tok#0"), f);

  bind(&m, add_rule(&g, "arg", "verbose-flag"), v);
  bind(&m, add_rule(&g, "arg", "debug-flag"), d);
  bind(&m, add_rule(&g, "arg", "width-option#0", "size-tok#0"), w);
  bind(&m, add_rule(&g, "arg", "height-option#0", "size-tok#0"), h);
  bind(&m, add_rule(&g, "arg", "file-option#0", "string-tok#0"), f);

  add_rule(&g, "start", "arg", "arg_list");
  add_rule(&g, "arg_list", "arg", "arg_list");
  add_rule(&g, "width-option#0", "width-option");
  add_rule(&g, "size-tok#0", "size-tok");
  add_rule(&g, "string-tok#0", "string-tok");
  add_rule(&g, "height-option#0", "height-option");
  add_rule(&g, "file-option#0", "file-option");

  const auto tokens = cfg::tokenize(&tbl, input);
  const auto chart = cfg::cyk(&g, &tokens, &m);

  if (!cfg::is_valid(&chart, cfg::get_start(&g)))
    return false;

  const auto trees = cfg::get_trees(&chart, cfg::get_start(&g));
  if (trees.size() > 1)
    l.logw("Multiple parse trees detected\n");

  cfg::run_actions(&trees.front(), &m);

  if (verbose_count > 1 || debug_count > 1 || width_count > 1 ||
      height_count > 1 || file_count > 1)
    return false;

  l = logger{c->log_level};
  l.logi("Parsing complete; the following variables have been set:\n");
  l.logs("\tverbose: true\n");
  l.logs("\tdebug: ", c->debug ? "true" : "false", "\n");
  l.logs("\twindow width: ", c->window_width, "\n");
  l.logs("\twindow height: ", c->window_height, "\n");
  l.logs("\tmatrix file: ", c->matrix_source_file, "\n");

  return true;
}

bool is_available(const std::vector<VkExtensionProperties> *v,
                  const std::string &n) {
  for (const auto &e : *v)
    if (n == e.extensionName)
      return true;
  return false;
}

bool is_available(const std::vector<VkLayerProperties> *v,
                  const std::string &n) {
  for (const auto &e : *v)
    if (n == e.layerName)
      return true;
  return false;
}
} // namespace
