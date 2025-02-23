#include "sigil.hpp"
#include <logger.hpp>

namespace {
bool record(context *c, uint32_t frame_index, uint32_t image_index);
bool submit(context *c, uint32_t frame_index, uint32_t image_index);
void present(context *c, uint32_t frame_index, uint32_t image_index);
} // namespace

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

namespace {
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
  vkCmdBindDescriptorSets(rb, VK_PIPELINE_BIND_POINT_GRAPHICS, c->layout.handle,
                          0, 1, &c->per_frame[c->frame_index].descriptor_set, 0,
                          0);

  VkDeviceSize offset{0};
  vkCmdBindVertexBuffers(rb, 0, 1, &c->vertex_buffer.handle, &offset);
  vkCmdSetViewport(rb, 0, 1, &c->viewport);
  vkCmdSetScissor(rb, 0, 1, &c->scissor);
  vkCmdDraw(rb, c->vertices.size(), 1, 0, 0);
  vkCmdEndRenderPass(rb);

  if (vkEndCommandBuffer(rb) != VK_SUCCESS) {
    l.loge("Failed to end command buffer\n");
    return false;
  }

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
} // namespace
