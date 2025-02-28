#include "sigil.hpp"
#include <cfgtk/lexer.hpp>
#include <cfgtk/parser.hpp>
#include <chrono>
#include <filesystem>
#include <query.hpp>
#include <shader.hpp>

namespace fs = std::filesystem;

namespace {
bool initialize(context *, const fs::path, const std::vector<std::string>);
bool parse_cli(context *c, const std::vector<std::string> *);
bool initialize_glfw(context *c);
bool create_instance(context *c);
bool select_physical(context *c);
bool create_device(context *c);
bool create_window(context *c);
bool create_surface(context *c);
bool create_swapchain(context *c);
bool create_image_views(context *c);
bool create_framebuffers(context *c);
bool create_render_pass(context *c);
bool create_pipeline(context *c);
bool create_semaphores(context *c);
bool create_command_buffers(context *c);

bool is_available(const std::vector<VkExtensionProperties> *,
                  const std::string &);
bool is_available(const std::vector<VkLayerProperties> *, const std::string &);

bool render(context *c);
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

  VkClearValue clear_value{};
  clear_value.color.float32[0] = 0.f;
  clear_value.color.float32[1] = 0.f;
  clear_value.color.float32[2] = 0.f;
  clear_value.color.float32[3] = 1.f;
  rp_begin_info.pClearValues = &clear_value;
  rp_begin_info.clearValueCount = 1;

  vkCmdBeginRenderPass(rb, &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(rb, VK_PIPELINE_BIND_POINT_GRAPHICS, c->pipeline.handle);

  VkViewport viewport{};
  viewport.width = (float)c->window_width;
  viewport.height = (float)c->window_height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  vkCmdSetViewport(rb, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent = {c->window_width, c->window_height};
  scissor.offset = {0, 0};
  vkCmdSetScissor(rb, 0, 1, &scissor);

  const uint32_t vertex_count{3};
  const uint32_t instance_count{1};
  const uint32_t first_vertex{0};
  const uint32_t first_instance{0};
  vkCmdDraw(rb, vertex_count, instance_count, first_vertex, first_instance);
  vkCmdEndRenderPass(rb);

  if (vkEndCommandBuffer(rb) != VK_SUCCESS) {
    l.loge("Failed to end command buffer\n");
    return false;
  }

  return true;
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

  if (!create_render_pass(c)) {
    l.loge("Render pass creation failed\n");
    return false;
  }

  if (!create_framebuffers(c)) {
    l.loge("Framebuffer creation failed\n");
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

  if (!create_command_buffers(c)) {
    l.loge("Command pool and buffer creation failed\n");
    return false;
  }

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
    VkImageView attachments[] = {c->image_views[i].handle};

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

bool conf_vertex_input_info(VkPipelineVertexInputStateCreateInfo *info) {
  info->sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  info->vertexBindingDescriptionCount = 0;
  info->pVertexBindingDescriptions = nullptr; // Optional
  info->vertexAttributeDescriptionCount = 0;
  info->pVertexAttributeDescriptions = nullptr; // Optional

  return true;
}

bool conf_assembly(VkPipelineInputAssemblyStateCreateInfo *a,
                   VkPipelineRasterizationStateCreateInfo *r,
                   VkPipelineMultisampleStateCreateInfo *m) {
  a->sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  a->topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  a->primitiveRestartEnable = VK_FALSE;

  r->sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  r->depthClampEnable = VK_FALSE;
  r->rasterizerDiscardEnable = VK_FALSE;
  r->polygonMode = VK_POLYGON_MODE_FILL;
  r->lineWidth = 1.0f;
  r->cullMode = VK_CULL_MODE_BACK_BIT;
  r->frontFace = VK_FRONT_FACE_CLOCKWISE;
  r->depthBiasEnable = VK_FALSE;
  r->depthBiasConstantFactor = 0.0f; // Optional
  r->depthBiasClamp = 0.0f;          // Optional
  r->depthBiasSlopeFactor = 0.0f;    // Optional
                                     //
  m->sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  m->sampleShadingEnable = VK_FALSE;
  m->rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  m->minSampleShading = 1.0f;          // Optional
  m->pSampleMask = nullptr;            // Optional
  m->alphaToCoverageEnable = VK_FALSE; // Optional
  m->alphaToOneEnable = VK_FALSE;      // Optional
  return true;
}

bool conf_color(VkPipelineColorBlendAttachmentState *state,
                VkPipelineColorBlendStateCreateInfo *blend) {
  state->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  state->blendEnable = VK_TRUE;
  state->srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;           // Optional
  state->dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
  state->colorBlendOp = VK_BLEND_OP_ADD;             // Optional
  state->srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
  state->dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
  state->alphaBlendOp = VK_BLEND_OP_ADD;             // Optional
                                                     //
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

bool create_layout(const VkDevice dev, VkPipelineLayout *layout) {
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = 0;            // Optional
  info.pSetLayouts = nullptr;         // Optional
  info.pushConstantRangeCount = 0;    // Optional
  info.pPushConstantRanges = nullptr; // Optional

  if (vkCreatePipelineLayout(dev, &info, nullptr, layout) != VK_SUCCESS)
    return false;

  return true;
}

bool create_pipeline(context *c) {
  logger l{c->log_level};

  const VkDevice device = c->device.handle;
  VkPipelineLayout layout{};
  if (!create_layout(device, &layout)) {
    l.loge("Failed to create pipeline layout\n");
    return false;
  }
  c->layout = raii::resource<adapter::vk_pipeline_layout>{device, layout};

  raii::resource<adapter::vk_shader_module> modules[2];
  VkPipelineShaderStageCreateInfo shader_stages[2];
  if (!conf_shaders(device, shader_stages, modules, c->log_level))
    return false;

  VkPipelineViewportStateCreateInfo viewport_state{};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  std::vector<VkDynamicState> dynamic_states{};
  VkViewport viewport{};
  VkRect2D scissor{};
  conf_dynamic_state(&dynamic_state, &dynamic_states, &viewport_state,
                     &viewport, &scissor, c->window_width, c->window_height);

  VkPipelineVertexInputStateCreateInfo vertex_input{};
  conf_vertex_input_info(&vertex_input);

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  VkPipelineMultisampleStateCreateInfo multisampling{};
  conf_assembly(&input_assembly, &rasterizer, &multisampling);

  VkPipelineColorBlendAttachmentState color_attachment{};
  VkPipelineColorBlendStateCreateInfo color_blending{};
  conf_color(&color_attachment, &color_blending);

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

  VkPipeline handle{};
  auto r = vkCreateGraphicsPipelines(device, 0, 1, &info, 0, &handle);
  if (r != VK_SUCCESS) {
    l.loge("Failed to create pipeline with code: " + std::to_string(r), "\n");
    return false;
  }

  c->pipeline = raii::resource<adapter::vk_pipeline>{device, handle};
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

  VkAttachmentReference attachment_ref{};
  attachment_ref.attachment = 0;
  attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &attachment_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  info.attachmentCount = 1;
  info.pAttachments = &attachment;
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

bool create_command_buffers(context *c) {
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
  cfg::add_entry(&tbl, cfg::token_type::option, "width-option", "-w|--width");
  cfg::add_entry(&tbl, cfg::token_type::option, "height-option", "-h|--height");
  cfg::add_entry(&tbl, cfg::token_type::free, "size-tok", "[1-9]\\d{2,3}");

  cfg::action_map_t m{};
  cfg::grammar_t g{};
  cfg::action_t f{};
  logger l{};

  unsigned verbose_count{}, debug_count{}, width_count{}, height_count{};

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

  bind(&m, add_rule(&g, "start", "help-flag"), help);
  bind(&m, add_rule(&g, "start", "verbose-flag"), v);
  bind(&m, add_rule(&g, "start", "debug-flag"), d);
  bind(&m, add_rule(&g, "start", "width-option#0", "size-tok#0"), w);
  bind(&m, add_rule(&g, "start", "height-option#0", "size-tok#0"), h);

  bind(&m, add_rule(&g, "arg_list", "verbose-flag"), v);
  bind(&m, add_rule(&g, "arg_list", "debug-flag"), d);
  bind(&m, add_rule(&g, "arg_list", "width-option#0", "size-tok#0"), w);
  bind(&m, add_rule(&g, "arg_list", "height-option#0", "size-tok#0"), h);

  bind(&m, add_rule(&g, "arg", "verbose-flag"), v);
  bind(&m, add_rule(&g, "arg", "debug-flag"), d);
  bind(&m, add_rule(&g, "arg", "width-option#0", "size-tok#0"), w);
  bind(&m, add_rule(&g, "arg", "height-option#0", "size-tok#0"), h);

  add_rule(&g, "start", "arg", "arg_list");
  add_rule(&g, "arg_list", "arg", "arg_list");
  add_rule(&g, "width-option#0", "width-option");
  add_rule(&g, "size-tok#0", "size-tok");
  add_rule(&g, "height-option#0", "height-option");

  const auto tokens = cfg::tokenize(&tbl, input);
  const auto chart = cfg::cyk(&g, &tokens, &m);

  if (!cfg::is_valid(&chart, cfg::get_start(&g)))
    return false;

  const auto trees = cfg::get_trees(&chart, cfg::get_start(&g));
  if (trees.size() > 1)
    l.logw("Multiple parse trees detected\n");

  cfg::run_actions(&trees.front(), &m);

  if (verbose_count > 1 || debug_count > 1 || width_count > 1 ||
      height_count > 1)
    return false;

  l = logger{c->log_level};
  l.logi("Parsing complete; the following variables have been set:\n");
  l.logs("\tverbose: true\n");
  l.logs("\tdebug: ", c->debug ? "true" : "false", "\n");
  l.logs("\twindow width: ", c->window_width, "\n");
  l.logs("\twindow height: ", c->window_height, "\n");

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
