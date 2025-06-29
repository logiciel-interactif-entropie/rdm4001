#pragma once
#include "base_types.hpp"
#include "imgui/imgui.h"
#include "vk_context.hpp"

namespace rdm::gfx::vk {
class VKTexture : public BaseTexture {
  VKContext* context;

 public:
  VKTexture(VKContext* ctxt);
  virtual ~VKTexture();

  virtual void reserve2d(int width, int height, InternalFormat format,
                         int mipmapLevels, bool renderbuffer);
  virtual void reserve2dMultisampled(int width, int height,
                                     InternalFormat format, int samples,
                                     bool renderbuffer);
  virtual void upload2d(int width, int height, DataType type, Format format,
                        void* data, int mipmapLevels);
  virtual void uploadCubeMap(int width, int height, std::vector<void*> data);
  virtual void destroyAndCreate();
  virtual void bind();

  virtual void setFiltering(Filtering min, Filtering max);
  virtual ImTextureID getImTextureId() { return 0; }
};

class VKProgram : public BaseProgram {
  VKContext* context;

 public:
  VKProgram(VKContext* ctxt);
  virtual ~VKProgram();

  virtual void bind();
  virtual void link();
};

class VKBuffer : public BaseBuffer {
  VKContext* context;
  size_t size;

 public:
  VKBuffer(VKContext* ctxt);
  virtual ~VKBuffer();

  virtual void upload(Type type, Usage usage, size_t size, const void* data);
  virtual void uploadSub(size_t offset, size_t size, const void* data);
  virtual void setBind(int index, size_t offset, size_t size);
  virtual void* lock(Type type, Access access);
  virtual void unlock(void* lock);
  virtual size_t getSize() { return size; }

  virtual void bind();
};

class VKArrayPointers : public BaseArrayPointers {
  VKContext* context;

 public:
  VKArrayPointers(VKContext* ctxt);
  virtual ~VKArrayPointers();

  virtual void upload();
  virtual void bind();
};

class VKFrameBuffer : public BaseFrameBuffer {
  VKContext* context;

 public:
  VKFrameBuffer(VKContext* ctxt);
  virtual ~VKFrameBuffer();

  virtual void setTarget(BaseTexture* texture, AttachmentPoint point);
  virtual Status getStatus();
  virtual void destroyAndCreate();
};
};  // namespace rdm::gfx::vk
