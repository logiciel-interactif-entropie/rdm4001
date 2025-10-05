#pragma once
#include <EGL/egl.h>
#include <glad/gl.h>

#include <mutex>

#include "base_context.hpp"

namespace rdm::gfx::gl {
class GLContext : public BaseContext {
  void updateVsync();

  EGLDisplay eglDisplay;
  EGLConfig eglConfig;
  EGLSurface windowSurface;
  EGLContext context;

 public:
  GLContext(AbstractionWindow* hwnd);

  virtual void setCurrent();
  virtual void unsetCurrent();
  virtual void swapBuffers();
  virtual glm::ivec2 getBufferSize();
};
};  // namespace rdm::gfx::gl
