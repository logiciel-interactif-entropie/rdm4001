#include "gl_device.hpp"

#include <glad/gl.h>

#include <cassert>
#include <format>
#include <stdexcept>

#include "apis.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gl_context.hpp"
#include "gl_types.hpp"
#include "logging.hpp"

namespace rdm::gfx::gl {
GLDevice::GLDevice(GLContext* context) : BaseDevice(context) {
  currentFrameBuffer = 0;
  numFramebufferDepth = 0;
}
void GLDevice::readPixels(int x, int y, int w, int h, void* d) {
  glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, d);
}

GLenum dssMapping[] = {GL_NEVER,  GL_ALWAYS,  GL_NEVER,    GL_LESS,  GL_EQUAL,
                       GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL};

void GLDevice::setDepthState(DepthStencilState s) {
  if (s == Disabled) {
    glDisable(GL_DEPTH_TEST);
  } else {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(dssMapping[s]);
  }
}

void GLDevice::setStencilState(DepthStencilState s) {}

GLenum bsMapping[] = {
    0,
    GL_ZERO,
    GL_ONE,
    GL_SRC_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_CONSTANT_COLOR,
    GL_ONE_MINUS_CONSTANT_COLOR,
    GL_CONSTANT_ALPHA,
    GL_ONE_MINUS_CONSTANT_ALPHA,
};

void GLDevice::setBlendState(BlendState a, BlendState b) {
  if (a == DDisabled || b == DDisabled) {
    glDisable(GL_BLEND);
  } else {
    glEnable(GL_BLEND);
    glBlendFunc(bsMapping[a], bsMapping[b]);
  }
}

void GLDevice::setBlendStateSeperate(BlendState aCol, BlendState bCol,
                                     BlendState aDep, BlendState bDep) {
  if (aCol == DDisabled || bCol == DDisabled || aDep == DDisabled ||
      bDep == DDisabled) {
    glDisable(GL_BLEND);
  } else {
    glEnable(GL_BLEND);
    glBlendFuncSeparate(bsMapping[aCol], bsMapping[bCol], bsMapping[aDep],
                        bsMapping[bDep]);
  }
}

void GLDevice::setCullState(CullState s) {
  switch (s) {
    case FrontCCW:
      glEnable(GL_CULL_FACE);
      glCullFace(GL_FRONT);
      glFrontFace(GL_CCW);
      break;
    case FrontCW:
      glEnable(GL_CULL_FACE);
      glCullFace(GL_FRONT);
      glFrontFace(GL_CW);
      break;
    case BackCCW:
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
      glFrontFace(GL_CCW);
      break;
    case BackCW:
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
      glFrontFace(GL_CW);
      break;
    case None:
      glDisable(GL_CULL_FACE);
      break;
  }
}

void GLDevice::clear(float r, float g, float b, float a) {
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

void GLDevice::clearDepth(float d) {
  glClearDepth(d);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void GLDevice::viewport(int x, int y, int w, int h) { glViewport(x, y, w, h); }

GLenum GLDevice::drawType(DrawType type) {
  switch (type) {
    case BaseDevice::Lines:
      return GL_LINES;
    default:
      return GL_TRIANGLES;
  }
}

void GLDevice::draw(BaseBuffer* base, DataType type, DrawType dtype,
                    size_t count, void* pointer) {
  base->bind();
  switch (dynamic_cast<GLBuffer*>(base)->getType()) {
    case BaseBuffer::Element:
      glDrawElements(drawType(dtype), count, fromDataType(type), pointer);
      break;
    case BaseBuffer::Array:
      glDrawArrays(drawType(dtype), (GLint)(size_t)pointer, count);
      break;
    case BaseBuffer::Unknown:
    default:
      throw std::runtime_error("Bad buffer type");
  }
}

void* GLDevice::bindFramebuffer(BaseFrameBuffer* buffer) {
  if (!buffer) throw std::runtime_error("NULL buffer");

  numFramebufferDepth++;
  assert(numFramebufferDepth < 100);

  dbgPushGroup(
      std::format("framebuffer {}", ((GLFrameBuffer*)buffer)->getId()));
  /*
  Log::printf(LOG_DEBUG, "Framebuffer %p (%s) bound", buffer,
              buffer->getTag().c_str());
  */

  void* oldFb = (void*)currentFrameBuffer;
  glBindFramebuffer(GL_FRAMEBUFFER, ((GLFrameBuffer*)buffer)->getId());
  currentFrameBuffer = buffer;
  return oldFb;
}

void GLDevice::unbindFramebuffer(void* p) {
  GLFrameBuffer* oldFb = (GLFrameBuffer*)p;

  /*  Log::printf(LOG_DEBUG,
              oldFb ? "Framebuffer %p (%s) bound, framebuffer %p (%s) unbound"
                    : "Returned to backbuffer fb",
              oldFb, oldFb ? oldFb->getTag().c_str() : NULL, currentFrameBuffer,
              currentFrameBuffer->getTag().c_str());
  */
  numFramebufferDepth--;

  glBindFramebuffer(GL_FRAMEBUFFER, oldFb ? oldFb->getId() : 0);
  currentFrameBuffer = oldFb;

  dbgPopGroup();
}

std::unique_ptr<BaseTexture> GLDevice::createTexture() {
  return std::unique_ptr<BaseTexture>(new GLTexture());
}

std::unique_ptr<BaseProgram> GLDevice::createProgram() {
  return std::unique_ptr<BaseProgram>(new GLProgram());
}

std::unique_ptr<BaseBuffer> GLDevice::createBuffer() {
  return std::unique_ptr<BaseBuffer>(new GLBuffer());
}

std::unique_ptr<BaseArrayPointers> GLDevice::createArrayPointers() {
  return std::unique_ptr<BaseArrayPointers>(new GLArrayPointers());
}

std::unique_ptr<BaseFrameBuffer> GLDevice::createFrameBuffer() {
  return std::unique_ptr<BaseFrameBuffer>(new GLFrameBuffer());
}

void GLDevice::targetAttachments(BaseFrameBuffer::AttachmentPoint* attachments,
                                 int count) {
  std::vector<GLenum> _attach;
  for (int i = 0; i < count; i++) {
    GLenum atp;
    switch (attachments[i]) {
      case BaseFrameBuffer::Depth:
        atp = GL_DEPTH_ATTACHMENT;
        break;
      case BaseFrameBuffer::Stencil:
        atp = GL_STENCIL_ATTACHMENT;
        break;
      case BaseFrameBuffer::DepthStencil:
        atp = GL_DEPTH_STENCIL_ATTACHMENT;
        break;
      default:  // should be colors
        atp = GL_COLOR_ATTACHMENT0 + attachments[i];
        break;
    }
    _attach.push_back(atp);
  }
  glDrawBuffers(count, _attach.data());
}

void GLDevice::dbgPushGroup(std::string message) {
#ifdef RDM_DBG_GL_DEBUG_GROUPS
  glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, message.size(),
                   message.c_str());
#endif
}

void GLDevice::dbgPopGroup() {
#ifdef RDM_DBG_GL_DEBUG_GROUPS
  glPopDebugGroup();
#endif
}
}  // namespace rdm::gfx::gl
