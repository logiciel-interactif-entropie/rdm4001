#include "vk_types.hpp"

#include "base_types.hpp"
#include "vk_context.hpp"
namespace rdm::gfx::vk {
// texture

VKTexture::VKTexture(VKContext* ctxt) { context = ctxt; }

VKTexture::~VKTexture() {}

void VKTexture::reserve2d(int width, int height, InternalFormat format,
                          int mipmapLevels, bool renderbuffer) {}

void VKTexture::reserve2dMultisampled(int width, int height,
                                      InternalFormat format, int samples,
                                      bool renderbuffer) {}

void VKTexture::upload2d(int width, int height, DataType type, Format format,
                         void* data, int mipmapLevels) {}

void VKTexture::uploadCubeMap(int width, int height, std::vector<void*> data) {}

void VKTexture::destroyAndCreate() {}

void VKTexture::bind() {}

void VKTexture::setFiltering(Filtering min, Filtering max) {}

// program

VKProgram::VKProgram(VKContext* ctxt) { context = ctxt; }

VKProgram::~VKProgram() {}

void VKProgram::bind() {}

void VKProgram::link() {}

// buffer

VKBuffer::VKBuffer(VKContext* ctxt) { context = ctxt; }

VKBuffer::~VKBuffer() {}

void VKBuffer::upload(Type type, Usage usage, size_t size, const void* data) {}

void VKBuffer::uploadSub(size_t offset, size_t size, const void* data) {}

void VKBuffer::setBind(int index, size_t offset, size_t size) {}

void* VKBuffer::lock(Type type, Access access) { return NULL; }

void VKBuffer::unlock(void* lock) {}

void VKBuffer::bind() {}

// array pointers

VKArrayPointers::VKArrayPointers(VKContext* ctxt) { this->context = ctxt; }

VKArrayPointers::~VKArrayPointers() {}

void VKArrayPointers::upload() {}

void VKArrayPointers::bind() {}

// frame buffer

VKFrameBuffer::VKFrameBuffer(VKContext* ctxt) { this->context = ctxt; }

VKFrameBuffer::~VKFrameBuffer() {}

void VKFrameBuffer::setTarget(BaseTexture* texture, AttachmentPoint point) {}

BaseFrameBuffer::Status VKFrameBuffer::getStatus() { return Complete; }

void VKFrameBuffer::destroyAndCreate() {}
}  // namespace rdm::gfx::vk
