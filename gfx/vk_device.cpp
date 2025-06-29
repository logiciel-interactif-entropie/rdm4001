#include "vk_device.hpp"

#include "base_device.hpp"
#include "base_types.hpp"
#include "vk_context.hpp"
#include "vk_types.hpp"

namespace rdm::gfx::vk {
VKDevice::VKDevice(VKContext* context) : BaseDevice(context) {
  this->context = context;
}

void VKDevice::setDepthState(DepthStencilState s) {}
void VKDevice::setStencilState(DepthStencilState s) {}
void VKDevice::setBlendState(BlendState a, BlendState b) {}
void VKDevice::setCullState(CullState s) {}

void VKDevice::clear(float r, float g, float b, float a) {}
void VKDevice::clearDepth(float d) {}
void VKDevice::viewport(int x, int y, int w, int h) {}

void VKDevice::draw(BaseBuffer* base, DataType type, DrawType dtype,
                    size_t count, void* pointer) {}
void* VKDevice::bindFramebuffer(BaseFrameBuffer* buffer) { return NULL; }
void VKDevice::unbindFramebuffer(void* p) {}

std::unique_ptr<BaseTexture> VKDevice::createTexture() {
  return std::unique_ptr<BaseTexture>(new VKTexture(context));
}

std::unique_ptr<BaseProgram> VKDevice::createProgram() {
  return std::unique_ptr<BaseProgram>(new VKProgram(context));
}

std::unique_ptr<BaseBuffer> VKDevice::createBuffer() {
  return std::unique_ptr<BaseBuffer>(new VKBuffer(context));
}

std::unique_ptr<BaseArrayPointers> VKDevice::createArrayPointers() {
  return std::unique_ptr<BaseArrayPointers>(new VKArrayPointers(context));
}

std::unique_ptr<BaseFrameBuffer> VKDevice::createFrameBuffer() {
  return std::unique_ptr<BaseFrameBuffer>(new VKFrameBuffer(context));
}

void VKDevice::targetAttachments(BaseFrameBuffer::AttachmentPoint* attachments,
                                 int count) {}

void VKDevice::startImGui() {}
void VKDevice::stopImGui() {}

void VKDevice::dbgPushGroup(std::string message) {}
void VKDevice::dbgPopGroup() {}
}  // namespace rdm::gfx::vk
