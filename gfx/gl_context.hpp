#pragma once
#include <SDL2/SDL.h>

#include <mutex>

#include "base_context.hpp"

namespace rdm::gfx::gl {
class GLContext : public BaseContext {
  SDL_GLContext context;

  void updateVsync();

 public:
  GLContext(void* hwnd);

  virtual void setCurrent();
  virtual void unsetCurrent();
  virtual void swapBuffers();
  virtual glm::ivec2 getBufferSize();

  static std::vector<DisplayMode> getSupportedDisplayModes();
  static int prepareSdl();

  SDL_GLContext getContext() { return context; }
};
};  // namespace rdm::gfx::gl
