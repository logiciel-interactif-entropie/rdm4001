#include "gl_context.hpp"

#include <glad/glad.h>

#include <stdexcept>

#include "SDL_video.h"
#include "logging.hpp"
#include "settings.hpp"

#ifndef DISABLE_EASY_PROFILER
#include <easy/profiler.h>
#endif

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
                                const GLchar* message, const void* userParam) {
  rdm::Log::printf(
      type == GL_DEBUG_TYPE_ERROR ? rdm::LOG_ERROR : rdm::LOG_EXTERNAL,
      "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s",
      (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity,
      message);
  if (type == GL_DEBUG_TYPE_ERROR) throw std::runtime_error(message);
}

namespace rdm::gfx::gl {
// we expect SDL's gl context
static CVar r_gldebug("r_gldebug", "0", CVARF_SAVE | CVARF_GLOBAL);
static CVar r_glvsync("r_glvsync", "0", CVARF_SAVE | CVARF_GLOBAL);
static CVar r_gladaptivevsync("r_gladaptivevsync", "1",
                              CVARF_SAVE | CVARF_GLOBAL);

void GLContext::updateVsync() {
  if (r_glvsync.getBool()) {
    Log::printf(LOG_INFO, "enabling %s vsync",
                r_gladaptivevsync.getBool() ? "adaptive" : "synchronized");

    bool success = SDL_GL_SetSwapInterval(r_gladaptivevsync.getBool() ? -1 : 1);

    if (!success) {
      if (r_gladaptivevsync.getBool()) {
        Log::printf(LOG_WARN,
                    "adaptive vsync unsupported, using synchronized vsync");
        SDL_GL_SetSwapInterval(1);
      }
    }
  } else {
    Log::printf(LOG_INFO, "disabling vsync");
    SDL_GL_SetSwapInterval(0);
  }
}

GLContext::GLContext(void* hwnd) : BaseContext(hwnd) {
  context = SDL_GL_CreateContext((SDL_Window*)hwnd);
  if (!context)
    Log::printf(LOG_FATAL, "Unable to create GLContext (%s)", SDL_GetError());

  setCurrent();
  if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
    Log::printf(LOG_FATAL, "Unable to initialize GLAD");
  }
  updateVsync();
  r_glvsync.changing.listen([this] { updateVsync(); });
  if (r_gldebug.getBool()) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, 0);
  }

  Log::printf(LOG_EXTERNAL, "Vendor:   %s", glGetString(GL_VENDOR));
  Log::printf(LOG_EXTERNAL, "Renderer: %s", glGetString(GL_RENDERER));
  Log::printf(LOG_EXTERNAL, "Version:  %s", glGetString(GL_VERSION));

  glClearColor(0.0, 0.0, 0.0, 0.0);  // clear because the first frame takes so
                                     // long it renders garbled graphics
  glClear(GL_COLOR_BUFFER_BIT);
  swapBuffers();
}

void GLContext::swapBuffers() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif
  SDL_GL_SwapWindow((SDL_Window*)getHwnd());
}

void GLContext::setCurrent() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif
  if (SDL_GL_MakeCurrent((SDL_Window*)getHwnd(), context)) {
    Log::printf(LOG_FATAL, "Unable to make context current (%s)",
                SDL_GetError());
  }
}

void GLContext::unsetCurrent() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif
  SDL_GL_MakeCurrent((SDL_Window*)getHwnd(), NULL);
}

glm::ivec2 GLContext::getBufferSize() {
  glm::ivec2 z;
  SDL_GetWindowSize((SDL_Window*)getHwnd(), &z.x, &z.y);
  return z;
}

int GLContext::prepareSdl() {
  int context_flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
#ifndef NDEBUG
  context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
#endif

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_LoadLibrary(NULL);

  return SDL_WINDOW_OPENGL;
}
}  // namespace rdm::gfx::gl
