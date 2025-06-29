#pragma once
#include <glad/glad.h>

#include "base_types.hpp"

namespace rdm::gfx::gl {
GLenum fromDataType(DataType t);

class GLTexture : public BaseTexture {
  GLuint texture;
  GLuint renderbuffer;
  bool isRenderBuffer;

 public:
  GLTexture();
  virtual ~GLTexture();

  GLuint getId() { return texture; }

  GLuint getRbId() { return renderbuffer; }

  bool getIsRenderBuffer() { return isRenderBuffer; }

  static GLenum texType(Type type);
  static GLenum texFormat(Format format);
  static GLenum texInternalFormat(InternalFormat format);

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

  virtual ImTextureID getImTextureId() { return getId(); }
};

class GLProgram : public BaseProgram {
  GLuint program;
  std::map<std::string, GLuint> locations;

 public:
  GLProgram();
  virtual ~GLProgram();

  GLuint getId() { return program; }

  static GLenum shaderType(Shader type);

  void bindParameters();

  virtual void bind();
  virtual void link();
};

class GLBuffer : public BaseBuffer {
  GLuint buffer;
  Type type;
  size_t size;
  void* _lock;

 public:
  GLBuffer();
  virtual ~GLBuffer();

  static GLenum bufType(Type type);
  static GLenum bufUsage(Usage usage);

  Type getType() { return type; }

  // will automagically use glBufferSubData
  virtual void upload(Type type, Usage usage, size_t size, const void* data);
  virtual void uploadSub(size_t offset, size_t size, const void* data);
  virtual void setBind(int index, size_t offset, size_t size);
  virtual void* lock(Type type, Access access);
  virtual void unlock(void* lock);
  virtual size_t getSize() { return size; }

  virtual void bind();
};

class GLArrayPointers : public BaseArrayPointers {
  GLuint array;

 public:
  GLArrayPointers();
  virtual ~GLArrayPointers();

  virtual void upload();
  virtual void bind();
};

class GLFrameBuffer : public BaseFrameBuffer {
  GLuint framebuffer;

 public:
  GLFrameBuffer();
  virtual ~GLFrameBuffer();

  virtual void setTarget(BaseTexture* texture, AttachmentPoint point);
  virtual Status getStatus();
  virtual void destroyAndCreate();

  GLuint getId() { return framebuffer; }
};
}  // namespace rdm::gfx::gl
