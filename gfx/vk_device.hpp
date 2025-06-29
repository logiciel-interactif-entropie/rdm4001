#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <vulkan/vulkan.hpp>

#include "base_device.hpp"
#include "vk_context.hpp"
namespace rdm::gfx::vk {
class VKDevice : public BaseDevice {
  VKContext* context;

 public:
  VKDevice(VKContext* context);

  virtual void setDepthState(DepthStencilState s);
  virtual void setStencilState(DepthStencilState s);
  virtual void setBlendState(BlendState a, BlendState b);
  virtual void setCullState(CullState s);

  virtual void clear(float r, float g, float b, float a);
  virtual void clearDepth(float d);
  virtual void viewport(int x, int y, int w, int h);

  virtual void draw(BaseBuffer* base, DataType type, DrawType dtype,
                    size_t count, void* pointer = 0);
  virtual void* bindFramebuffer(BaseFrameBuffer* buffer);
  virtual void unbindFramebuffer(void* p);

  virtual std::unique_ptr<BaseTexture> createTexture();
  virtual std::unique_ptr<BaseProgram> createProgram();
  virtual std::unique_ptr<BaseBuffer> createBuffer();
  virtual std::unique_ptr<BaseArrayPointers> createArrayPointers();
  virtual std::unique_ptr<BaseFrameBuffer> createFrameBuffer();

  virtual void targetAttachments(BaseFrameBuffer::AttachmentPoint* attachments,
                                 int count);

  virtual void startImGui();
  virtual void stopImGui();

  virtual void dbgPushGroup(std::string message);
  virtual void dbgPopGroup();
};
}  // namespace rdm::gfx::vk
