#include "gl_context.hpp"

#include <EGL/egl.h>
#include <EGL/eglplatform.h>
#define GLAD_EGL_IMPLEMENTATION
#include <glad/egl.h>

#include <stdexcept>

#include "SDL3/SDL_video.h"
#include "base_context.hpp"
#include "logging.hpp"
#include "settings.hpp"
#include "window.hpp"

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

    bool success =
        eglSwapInterval(eglDisplay, r_gladaptivevsync.getBool() ? -1 : 1);

    if (!success) {
      if (r_gladaptivevsync.getBool()) {
        Log::printf(LOG_WARN,
                    "adaptive vsync unsupported, using synchronized vsync");
        eglSwapInterval(eglDisplay, 1);
      }
    }
  } else {
    Log::printf(LOG_INFO, "disabling vsync");
    eglSwapInterval(eglDisplay, 0);
  }
}

#define COLOR_SPACE EGL_COLORSPACE_sRGB
#define ALPHA_FORMAT EGL_ALPHA_FORMAT_NONPRE
#define SURFACE_TYPE EGL_WINDOW_BIT | EGL_PBUFFER_BIT

static const EGLint s_surfaceAttribs[] = {
    EGL_COLORSPACE, COLOR_SPACE, EGL_ALPHA_FORMAT, ALPHA_FORMAT, EGL_NONE};

static const EGLint s_configAttribs[] = {EGL_RED_SIZE,
                                         8,
                                         EGL_GREEN_SIZE,
                                         8,
                                         EGL_BLUE_SIZE,
                                         8,
                                         EGL_ALPHA_SIZE,
                                         8,
                                         EGL_LUMINANCE_SIZE,
                                         EGL_DONT_CARE,
                                         EGL_SURFACE_TYPE,
                                         SURFACE_TYPE,
                                         EGL_RENDERABLE_TYPE,
                                         EGL_OPENVG_BIT& EGL_OPENGL_ES_BIT,
                                         EGL_BIND_TO_TEXTURE_RGBA,
                                         EGL_TRUE,
                                         EGL_NONE};

/*static const EGLint s_pbufferAttribs[] = {
  EGL_WIDTH,        PBUFFER_WIDTH,      EGL_HEIGHT,
  PBUFFER_HEIGHT,   EGL_COLORSPACE,     COLOR_SPACE,
  EGL_ALPHA_FORMAT, ALPHA_FORMAT,       EGL_TEXTURE_FORMAT,
  EGL_TEXTURE_RGBA, EGL_TEXTURE_TARGET, EGL_TEXTURE_2D,
  EGL_NONE};
*/

GLContext::GLContext(AbstractionWindow* hwnd) : BaseContext(hwnd) {
  int eglVersion = gladLoaderLoadEGL(NULL);
  if (!eglVersion) {
    throw std::runtime_error("Unable to load EGL");
  }
  Log::printf(LOG_DEBUG, "Loaded EGL %d.%d", GLAD_VERSION_MAJOR(eglVersion),
              GLAD_VERSION_MINOR(eglVersion));
  eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (eglDisplay == EGL_NO_DISPLAY) {
    throw std::runtime_error("No EGL display");
  }
  EGLint major, minor;
  if (!eglInitialize(eglDisplay, &major, &minor)) {
    throw std::runtime_error("Could not initialize EGL");
  }
  eglVersion = gladLoaderLoadEGL(eglDisplay);
  if (!eglVersion) {
    throw std::runtime_error("Unable to load EGL (2)");
  }
  Log::printf(LOG_DEBUG, "Loaded EGL %d.%d after reload",
              GLAD_VERSION_MAJOR(eglVersion), GLAD_VERSION_MINOR(eglVersion));

  EGLint numConfigs;
  eglGetConfigs(eglDisplay, NULL, 0, &numConfigs);
  eglChooseConfig(eglDisplay, s_configAttribs, &eglConfig, 1, &numConfigs);
  windowSurface = eglCreateWindowSurface(eglDisplay, eglConfig,
                                         (NativeWindowType)hwnd->getGfxHwnd(),
                                         s_surfaceAttribs);

  EGLint attributes[] = {
      EGL_CONTEXT_MAJOR_VERSION,
      3,
      EGL_CONTEXT_MINOR_VERSION,
      3,
      EGL_CONTEXT_OPENGL_PROFILE_MASK,
      EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#ifndef NDEBUG
      EGL_CONTEXT_OPENGL_DEBUG,
      EGL_TRUE,
#endif
      EGL_NONE,
  };

  eglBindAPI(EGL_OPENGL_API);
  context = eglCreateContext(eglDisplay, eglConfig, NULL, attributes);
  if (!context) {
    throw std::runtime_error("Unable to create EGL context");
  }

  setCurrent();
  int glVersion = gladLoaderLoadGL();
  if (!glVersion) {
    throw std::runtime_error("Unable to load GL");
  }
  Log::printf(LOG_DEBUG, "Loaded GL %d.%d", GLAD_VERSION_MAJOR(glVersion),
              GLAD_VERSION_MINOR(glVersion));
  r_glvsync.changing.listen([this] { updateVsync(); });
  if (r_gldebug.getBool()) {
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, 0);
  }

  Log::printf(LOG_DEBUG, "Vendor:   %s", glGetString(GL_VENDOR));
  Log::printf(LOG_DEBUG, "Renderer: %s", glGetString(GL_RENDERER));
  Log::printf(LOG_DEBUG, "Version:  %s", glGetString(GL_VERSION));

  glEnable(GL_DITHER);
  glClearColor(0.0, 0.0, 0.0, 0.0);  // clear because the first frame takes so
                                     // long it renders garbled graphics
  glClear(GL_COLOR_BUFFER_BIT);
  swapBuffers();

  updateVsync();
}

void GLContext::swapBuffers() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif
  eglSwapBuffers(eglDisplay, windowSurface);
}

void GLContext::setCurrent() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif
  if (!eglMakeCurrent(eglDisplay, windowSurface, windowSurface, context)) {
    Log::printf(LOG_ERROR, "eglMakeCurrent returned %04x", eglGetError());
  }
}

void GLContext::unsetCurrent() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif
  eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

glm::ivec2 GLContext::getBufferSize() { return getHwnd()->getWindowSize(); }
}  // namespace rdm::gfx::gl
