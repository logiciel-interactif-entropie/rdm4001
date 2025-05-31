#include "gl_types.hpp"

#include <format>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <vector>

#include "gfx/base_types.hpp"
#include "glad/glad.h"
#include "logging.hpp"

namespace rdm::gfx::gl {
GLenum fromDataType(DataType t) {
  switch (t) {
    case DtUnsignedByte:
      return GL_UNSIGNED_BYTE;
    case DtByte:
      return GL_BYTE;
    case DtUnsignedShort:
      return GL_UNSIGNED_SHORT;
    case DtShort:
      return GL_SHORT;
    case DtUnsignedInt:
      return GL_UNSIGNED_INT;
    case DtInt:
      return GL_INT;
    case DtFloat:
      return GL_FLOAT;
    default:
      throw std::runtime_error("Invalid type");
  }
}

GLTexture::GLTexture() {
  glGenTextures(1, &texture);
  isRenderBuffer = false;
}

GLTexture::~GLTexture() { glDeleteTextures(1, &texture); }

GLenum GLTexture::texType(BaseTexture::Type type) {
  switch (type) {
    case Texture2D:
      return GL_TEXTURE_2D;
      break;
    case Texture2D_MultiSample:
      return GL_TEXTURE_2D_MULTISAMPLE;
      break;
    case CubeMap:
      return GL_TEXTURE_CUBE_MAP;
      break;
    default:
      throw std::runtime_error("Invalid type");
  }
}

GLenum GLTexture::texFormat(Format format) {
  switch (format) {
    case RGB:
      return GL_RGB;
    case RGBA:
      return GL_RGBA;
    default:
      throw std::runtime_error("Invalid type");
  }
}

GLenum GLTexture::texInternalFormat(InternalFormat format) {
  switch (format) {
    case RGB8:
      return GL_RGB8;
    case RGBA8:
      return GL_RGBA8;
    case RGBF32:
      return GL_RGB32F;
    case RGBAF32:
      return GL_RGBA32F;
    case D24S8:
      return GL_DEPTH24_STENCIL8;
    default:
      throw std::runtime_error("Invalid type");
  }
}

void GLTexture::reserve2d(int width, int height, InternalFormat format,
                          int mipmapLevels, bool renderbuffer) {
  textureType = Texture2D;
  textureFormat = format;

  if (renderbuffer) {
    glGenRenderbuffers(1, &this->renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, this->renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, texInternalFormat(format), width,
                          height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    isRenderBuffer = true;
  } else {
    GLenum target = texType(textureType);
    glBindTexture(target, texture);
    glTexStorage2D(target, 0, texInternalFormat(textureFormat), width, height);
    if (mipmapLevels != 0) {
      glGenerateMipmap(target);
    }
    glBindTexture(target, 0);
  }
}

void GLTexture::reserve2dMultisampled(int width, int height,
                                      InternalFormat format, int samples,
                                      bool renderbuffer) {
  textureType = Texture2D_MultiSample;
  textureFormat = format;

  if (renderbuffer) {
    glGenRenderbuffers(1, &this->renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, this->renderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                     texInternalFormat(format), width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    isRenderBuffer = true;
  } else {
    GLenum target = texType(textureType);
    glBindTexture(target, texture);
    glTexImage2DMultisample(target, samples, texInternalFormat(textureFormat),
                            width, height, GL_TRUE);
    glBindTexture(target, 0);
  }
}

void GLTexture::upload2d(int width, int height, DataType type,
                         BaseTexture::Format format, void* data,
                         int mipmapLevels) {
  textureType = Texture2D;

  switch (format) {
    case RGB:
      textureFormat = RGB8;
      break;
    case RGBA:
      textureFormat = RGBA8;
      break;
    default:
      throw std::runtime_error("Invalid type");
  }

  GLenum target = texType(textureType);

  glBindTexture(target, texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  glTexImage2D(target, 0, texInternalFormat(textureFormat), width, height, 0,
               texFormat(format), fromDataType(type), data);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  if (mipmapLevels) {
    glGenerateMipmap(target);
  }
  glBindTexture(target, 0);
}

void GLTexture::uploadCubeMap(int width, int height, std::vector<void*> data) {
  textureType = CubeMap;
  GLenum target = texType(textureType);
  glBindTexture(target, texture);
  if (data.size() != 6) {
    throw std::runtime_error(
        "std::vector<void*> data.size() must be equal to 6, fill unchanged "
        "elements with NULL");
  }

  GLenum dt[] = {
      GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
      GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z};
  for (int i = 0; i < 6; i++) {
    glTexImage2D(dt[i], 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 data[i]);
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glBindTexture(target, 0);
}

void GLTexture::setFiltering(Filtering min, Filtering max) {
  GLenum filterTypes[] = {GL_NEAREST, GL_LINEAR};

  GLenum target = texType(textureType);
  glBindTexture(target, texture);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filterTypes[min]);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filterTypes[max]);
  glBindTexture(target, 0);
}

void GLTexture::destroyAndCreate() {
  glDeleteTextures(1, &texture);
  glGenTextures(1, &texture);
  if (isRenderBuffer) {
    glDeleteRenderbuffers(1, &renderbuffer);
    isRenderBuffer = false;
  }
}

void GLTexture::bind() {
  GLenum target = texType(textureType);
  glBindTexture(target, texture);
}

GLProgram::GLProgram() { program = glCreateProgram(); }

GLProgram::~GLProgram() { glDeleteProgram(program); }

GLenum GLProgram::shaderType(Shader type) {
  switch (type) {
    case Vertex:
      return GL_VERTEX_SHADER;
    case Fragment:
      return GL_FRAGMENT_SHADER;
    case Geometry:
      return GL_GEOMETRY_SHADER;
    default:
      break;
  }
}

void GLProgram::link() {
  std::vector<GLuint> _shaders;
  std::string programName;
  for (auto [type, shader] : shaders) {
    Log::printf(LOG_DEBUG, "Compiling shader %s", shader.name.c_str());

    GLuint _shader = glCreateShader(shaderType(type));
    glObjectLabel(GL_SHADER, _shader, shader.name.size(), shader.name.data());
    programName += shader.name + " ";
    GLchar* code = (GLchar*)shader.code.c_str();
    int codeLength[] = {(int)shader.code.size()};
    glShaderSource(_shader, 1, &code, (const GLint*)&codeLength);
    glCompileShader(_shader);

    GLint success = 0;
    glGetShaderiv(_shader, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
      GLint logSize = 0;
      glGetShaderiv(_shader, GL_INFO_LOG_LENGTH, &logSize);
      char* infoLog = (char*)malloc(logSize);
      glGetShaderInfoLog(_shader, logSize, NULL, infoLog);
      Log::printf(LOG_ERROR, "Shader compile %s error\n%s", shader.name.c_str(),
                  infoLog);
#ifndef NDEBUG
      Log::printf(LOG_DEBUG, "Shader code:\n%s", shader.code.c_str());
#endif
      free(infoLog);

      throw std::runtime_error("Shader compile error");
    } else {
      Log::printf(LOG_DEBUG, "Successfully compiled shader %s",
                  shader.name.c_str());
    }

    glAttachShader(program, _shader);
    _shaders.push_back(_shader);
  }

  glObjectLabel(GL_PROGRAM, program, programName.size(), programName.data());
  glLinkProgram(program);

  GLint success = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (success == GL_FALSE) {
    GLint logSize = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
    char* infoLog = (char*)malloc(logSize);
    glGetProgramInfoLog(program, logSize, NULL, infoLog);
    Log::printf(LOG_ERROR, "Program link error\n%s", infoLog);
    free(infoLog);

    throw std::runtime_error("Program link error");
  }

  for (auto shader : _shaders) {
    glDeleteShader(shader);
  }
}

void GLProgram::bindParameters() {
  for (auto& [name, pair] : parameters) {
    GLuint object;
    if (locations.find(name) != locations.end()) {
      object = locations[name];
    } else {
      object = glGetUniformLocation(program, name.c_str());
      locations[name] = object;
    }

    if (!pair.first.dirty) continue;
    switch (pair.first.type) {
      case DtInt:
        glUniform1i(object, pair.second.integer);
        break;
      case DtMat2:
        glUniformMatrix2fv(object, 1, false,
                           glm::value_ptr(pair.second.matrix2x2));
        break;
      case DtMat3:
        glUniformMatrix3fv(object, 1, false,
                           glm::value_ptr(pair.second.matrix3x3));
        break;
      case DtMat4:
        glUniformMatrix4fv(object, 1, false,
                           glm::value_ptr(pair.second.matrix4x4));
        break;
      case DtVec2:
        glUniform2fv(object, 1, glm::value_ptr(pair.second.vec2));
        break;
      case DtVec3:
        glUniform3fv(object, 1, glm::value_ptr(pair.second.vec3));
        break;
      case DtVec4:
        glUniform4fv(object, 1, glm::value_ptr(pair.second.vec4));
        break;
      case DtFloat:
        glUniform1fv(object, 1, &pair.second.number);
        break;
      case DtSampler:
        if (pair.second.texture.texture) {
          glActiveTexture(GL_TEXTURE0 + pair.second.texture.slot);
          pair.second.texture.texture->bind();
          glUniform1i(object, pair.second.texture.slot);
        }
        break;
      default:
        throw std::runtime_error("FIX THIS!! bad datatype for parameter");
        break;
    }
    pair.first.dirty = false;
  }
}

void GLProgram::bind() {
  glUseProgram(program);
  bindParameters();
}

GLBuffer::GLBuffer() { glCreateBuffers(1, &buffer); }

GLBuffer::~GLBuffer() { glDeleteBuffers(1, &buffer); }

GLenum GLBuffer::bufType(Type type) {
  switch (type) {
    case Element:
      return GL_ELEMENT_ARRAY_BUFFER;
    case Array:
      return GL_ARRAY_BUFFER;
    default:
      throw std::runtime_error("GL bufType(Type type) used with bad type");
  }
}

GLenum GLBuffer::bufUsage(Usage usage) {
  switch (usage) {
    case StaticDraw:
      return GL_STATIC_DRAW;
    default:
      return GL_DYNAMIC_DRAW;
  }
}

void GLBuffer::upload(Type type, Usage usage, size_t size, const void* data) {
  this->type = type;
  glBindBuffer(bufType(type), buffer);
  if (this->size != size) {
    glBufferData(bufType(type), size, data, bufUsage(usage));
    this->size = size;
  } else {
    glBufferSubData(bufType(type), 0, size, data);
  }
  glBindBuffer(bufType(type), 0);
}

void* GLBuffer::lock(Type type, Access access) {
  if (_lock) {
    throw std::runtime_error("Lock already locked buffer");
  }

  GLenum accesses[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};

  glBindBuffer(bufType(type), buffer);
  _lock = glMapBuffer(bufType(type), accesses[access]);
  glBindBuffer(bufType(type), 0);

  return _lock;
}

void GLBuffer::unlock(void* lock) {
  if (_lock != lock) {
    throw std::runtime_error("Lock with invalid lock");
  }

  glBindBuffer(bufType(type), buffer);
  glUnmapBuffer(bufType(type));
  glBindBuffer(bufType(type), 0);
}

void GLBuffer::bind() { glBindBuffer(bufType(type), buffer); }

GLArrayPointers::GLArrayPointers() { glCreateVertexArrays(1, &array); }

GLArrayPointers::~GLArrayPointers() { glDeleteVertexArrays(1, &array); }

void GLArrayPointers::upload() {
  glBindVertexArray(array);
  for (auto attrib : attribs) {
    if (attrib.buffer &&
        dynamic_cast<GLBuffer*>(attrib.buffer)->getType() == GLBuffer::Array) {
      attrib.buffer->bind();
    }
    glEnableVertexAttribArray(attrib.layoutId);

    // HACKHACKHACK: HACK
    if (attrib.type == DtInt || attrib.type == DtUnsignedInt)
      glVertexAttribIPointer(attrib.layoutId, attrib.size,
                             fromDataType(attrib.type), attrib.stride,
                             attrib.offset);
    else
      glVertexAttribPointer(attrib.layoutId, attrib.size,
                            fromDataType(attrib.type), attrib.normalized,
                            attrib.stride, attrib.offset);
  }
  glBindVertexArray(0);
}

void GLArrayPointers::bind() { glBindVertexArray(array); }

GLFrameBuffer::GLFrameBuffer() { glGenFramebuffers(1, &framebuffer); }

GLFrameBuffer::~GLFrameBuffer() { glDeleteFramebuffers(1, &framebuffer); }

void GLFrameBuffer::setTarget(BaseTexture* texture, AttachmentPoint point) {
  GLTexture* _gltexture = dynamic_cast<GLTexture*>(texture);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  GLenum atp;
  std::string atpnam = "";
  switch (point) {
    case Depth:
      atp = GL_DEPTH_ATTACHMENT;
      atpnam = "Depth attachment";
      break;
    case Stencil:
      atp = GL_STENCIL_ATTACHMENT;
      atpnam = "Stencil attachment";
      break;
    case DepthStencil:
      atp = GL_DEPTH_STENCIL_ATTACHMENT;
      atpnam = "Depth & Stencil attachment";
      break;
    default:  // should be colors
      atp = GL_COLOR_ATTACHMENT0 + point;
      atpnam = std::format("Color attachment {}", (int)point);
      break;
  }
  if (_gltexture->getIsRenderBuffer())
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, atp, GL_RENDERBUFFER,
                              _gltexture->getRbId());
  else
    glFramebufferTexture(GL_FRAMEBUFFER, atp, _gltexture->getId(), 0);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    const char* msgs[] = {"Missing attachment", "Bad dimensions",
                          "Bad attachment", "Complete", "Unsupported"};

    Log::printf(LOG_ERROR, "Framebuffer status: %04x (%s, attaching %s)",
                status, msgs[getStatus()], atpnam.c_str());
    throw std::runtime_error("Incomplete framebuffer");
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

BaseFrameBuffer::Status GLFrameBuffer::getStatus() {
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
    default:
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      return BadAttachment;
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
      return BadDimensions;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      return MissingAttachment;
    case GL_FRAMEBUFFER_COMPLETE:
      return Complete;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      return Unsupported;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLFrameBuffer::destroyAndCreate() {
  glDeleteFramebuffers(1, &framebuffer);
  glCreateFramebuffers(1, &framebuffer);
}
}  // namespace rdm::gfx::gl
