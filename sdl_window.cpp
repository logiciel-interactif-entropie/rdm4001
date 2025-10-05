#include "sdl_window.hpp"

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <filesystem.hpp>

#include "gfx/base_context.hpp"
#include "gfx/stb_image.h"
#include "input.hpp"
#include "logging.hpp"
#include "settings.hpp"
#include "window.hpp"

namespace rdm {
SDLWindow::SDLWindow(Game* game) : AbstractionWindow(game) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    Log::printf(LOG_FATAL, "Unable to init SDL (%s)", SDL_GetError());
    throw std::runtime_error("SDL init failed");
  }

  ignoreNextMouseMoveEvent = false;

  glm::vec2 wsize =
      Settings::singleton()->getCvar("cl_savedwindowsize")->getVec2();
  int flags = 0;
  std::string r_api = Settings::singleton()->getCvar("r_api")->getValue();
  if (r_api == "GLModern") {
    /*
      int context_flags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
#ifndef NDEBUG
    context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_LoadLibrary(NULL);

    flags |= SDL_WINDOW_OPENGL;*/
  } else if (r_api == "Vulkan") {
    SDL_Vulkan_LoadLibrary(NULL);
    flags |= SDL_WINDOW_VULKAN;
  }

  flags |= Settings::singleton()->getCvar("fullscreen")->getBool()
               ? SDL_WINDOW_FULLSCREEN
               : SDL_WINDOW_RESIZABLE;
  window = SDL_CreateWindow("RDM4001!!!", wsize.x, wsize.y, flags);
  SDL_SetWindowMinimumSize(window, 800, 600);

  if (!window) {
    Log::printf(LOG_FATAL, "Unable to create Window (%s)", SDL_GetError());
    throw std::runtime_error("SDL window couldn't be created");
  }
};

SDLWindow::~SDLWindow() {}

void SDLWindow::eventLoop() {
  CVar* input_enableimgui = Settings::singleton()->getCvar("input_enableimgui");
  CVar* cl_savedwindowsize =
      Settings::singleton()->getCvar("cl_savedwindowsize");
  CVar* cl_savedwindowpos = Settings::singleton()->getCvar("cl_savedwindowpos");
  CVar* input_userelativemode =
      Settings::singleton()->getCvar("input_userelativemode");

  bool ignoreMouse = false;
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (input_enableimgui->getBool()) {
      // std::scoped_lock l(gfxEngine->getImguiLock());
      // ImGui_ImplSDL3_NewFrame();
      // ImGui_ImplSDL3_ProcessEvent(&event);
    }
    InputObject object;

    switch (event.type) {
      case SDL_EVENT_WINDOW_RESIZED:
        cl_savedwindowsize->setVec2(
            glm::vec2(event.window.data1, event.window.data2));
        // ImGui::GetIO().DisplaySize =
        //     ImVec2(event.window.data1, event.window.data2);
        break;
      case SDL_EVENT_WINDOW_MOVED:
        cl_savedwindowpos->setVec2(
            glm::vec2(event.window.data1, event.window.data2));
        break;
      case SDL_EVENT_TEXT_INPUT:
        if (!(SDL_GetModState() & SDL_KMOD_CTRL &&
              (event.text.text[0] == 'c' || event.text.text[0] == 'C' ||
               event.text.text[0] == 'v' || event.text.text[0] == 'V'))) {
          std::string& text = Input::singleton()->getEditedText();
          text += event.text.text;
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        object.type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                          ? InputObject::MousePress
                          : InputObject::MouseUp;
        object.data.mouse.mouse_down = object.type == InputObject::MousePress;
        object.data.mouse.button = event.button.button;
        Input::singleton()->postEvent(object);
        break;
      case SDL_EVENT_KEY_UP:
      case SDL_EVENT_KEY_DOWN:
        if (Input::singleton()->isEditingText()) {
          if (event.type != SDL_EVENT_KEY_DOWN) break;
          std::string& text = Input::singleton()->getEditedText();
          if (event.key.key == SDLK_BACKSPACE && text.length() != 0) {
            text.pop_back();
          } else if (event.key.key == SDLK_C &&
                     SDL_GetModState() & SDL_KMOD_CTRL) {
            SDL_SetClipboardText(text.c_str());
          } else if (event.key.key == SDLK_V &&
                     SDL_GetModState() & SDL_KMOD_CTRL) {
            char* temp = SDL_GetClipboardText();
            text = temp;
            SDL_free(temp);
          } else if (event.key.key == SDLK_RETURN) {
            Input::singleton()->stopEditingText();
          }
        } else {
          object.type = event.type == SDL_EVENT_KEY_DOWN ? InputObject::KeyPress
                                                         : InputObject::KeyUp;
          object.data.key.key = event.key.key;
          Input::singleton()->postEvent(object);
        }
        break;
      case SDL_EVENT_QUIT:
        object.type = InputObject::Quit;
        Input::singleton()->postEvent(object);
        break;
      case SDL_EVENT_MOUSE_MOTION:
        if (ignoreMouse) break;
        if (ignoreNextMouseMoveEvent) {
          ignoreNextMouseMoveEvent = false;
          break;
        }
        object.type = InputObject::MouseMove;
        object.data.mouse.delta[0] = event.motion.xrel;
        object.data.mouse.delta[1] = event.motion.yrel;

        if (Input::singleton()->getMouseLocked()) {
          int w, h;
          SDL_GetWindowSize(window, &w, &h);
          if (input_userelativemode->getBool()) {
            SDL_SetWindowRelativeMouseMode(window, true);
            object.data.mouse.position[0] = w / 2;
            object.data.mouse.position[1] = h / 2;
          } else {
            SDL_SetWindowRelativeMouseMode(window, false);
            SDL_WarpMouseInWindow(window, w / 2, h / 2);
            object.data.mouse.position[0] = w / 2;
            object.data.mouse.position[1] = h / 2;
            ignoreNextMouseMoveEvent = true;
          }
        } else {
          if (input_userelativemode->getBool())
            SDL_SetWindowRelativeMouseMode(window, false);
          object.data.mouse.position[0] = event.motion.x;
          object.data.mouse.position[1] = event.motion.y;
        }
        Input::singleton()->postEvent(object);
        break;
      default:
        break;
    }
  }
  /*if (Input::singleton()->getMouseLocked()) {
    SDL_SetRelativeMouseMode(
    Input::singleton()->getMouseLocked() ? SDL_TRUE : SDL_FALSE);
    }*/
}

void SDLWindow::setTitle(std::string title) {};

/* GIMP RGB C-Source image dump */

static const struct {
  unsigned int width;
  unsigned int height;
  unsigned int bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  char* comment;
  unsigned char pixel_data[16 * 16 * 2 + 1];
} badIcon = {
    16,
    16,
    2,
    "Hello :D",
    "\377\377\377\377\377\377\377\377\377\377\000\000\000\000\000\000\000\000"
    "\000\000\000\000\000\000\377\377\377\377\377\377\377\377\377\377\377\377"
    "\377\377\377\377\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
    "\000\000\000\000\377\377\377\377\377\377\377\377\377\377\377\377\000\000"
    "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\377\377\000\000"
    "\000\000\377\377\377\377\377\377\377\377\377\377\000\000\000\000\377\377"
    "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\000\000\377\377"
    "\377\377\377\377\377\377\377\377\000\000\000\000\377\377\377\377\377\377"
    "\377\377\377\377\377\377\377\377\377\377\000\000\377\377\377\377\377\377"
    "\377\377\377\377\000\000\377\377\377\377\377\377\377\377\377\377\377\377"
    "\377\377\377\377\377\377\000\000\377\377\377\377\377\377\377\377\377\377"
    "\000\000\000\000\000\000\000\000\377\377\377\377\377\377\000\000\000\000"
    "\000\000\000\000\377\377\377\377\377\377\377\377\377\377\000\000\377\377"
    "\000\000\000\000\000\000\377\377\000\000\377\377\000\000\377\377\000\000"
    "\377\377\377\377\377\377\377\377\377\377\000\000\377\377\377\377\377\377"
    "\000\000\377\377\377\377\377\377\377\377\377\377\000\000\377\377\377\377"
    "\377\377\377\377\377\377\377\377\000\000\377\377\000\000\000\000\377\377"
    "\377\377\000\000\377\377\000\000\377\377\377\377\377\377\377\377\377\377"
    "\377\377\377\377\000\000\000\000\377\377\000\000\000\000\000\000\377\377"
    "\000\000\000\000\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
    "\377\377\000\000\000\000\377\377\377\377\377\377\000\000\377\377\000\000"
    "\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\377\000\000"
    "\000\000\000\000\000\000\000\000\377\377\000\000\377\377\377\377\377\377"
    "\377\377\377\377\377\377\000\000\377\377\000\000\000\000\000\000\377\377"
    "\377\377\377\377\377\377\000\000\377\377\377\377\377\377\377\377\377\377"
    "\377\377\000\000\000\000\000\000\377\377\377\377\000\000\000\000\000\000"
    "\000\000\377\377\377\377\377\377\377\377\377\377\377\377\377\377\000\000"
    "\000\000\000\000\377\377\377\377\377\377\377\377\377\377\377\377\377\377"
    "\377\377\377\377\377\377\377",
};

void SDLWindow::updateIcon(std::string str) {
  auto imgData = common::FileSystem::singleton()->readFile(str.c_str());
  void* data = (void*)badIcon.pixel_data;
  int x, y, ch, pitch;
  x = badIcon.width;
  y = badIcon.height;
  pitch = badIcon.bytes_per_pixel * x;
  ch = 3;
  SDL_PixelFormat fmt = SDL_PIXELFORMAT_RGB565;

  if (imgData) {
    stbi_set_flip_vertically_on_load(false);
    data = (void*)stbi_load_from_memory(imgData->data(), imgData->size(), &x,
                                        &y, &ch, 4);
    pitch = ((x * ch) + 3) & ~3;
    fmt = ch == 4 ? SDL_PIXELFORMAT_ARGB8888 : SDL_PIXELFORMAT_RGBX8888;
  } else {
    Log::printf(LOG_ERROR, "Could not load icon %s", str.c_str());
  }

  SDL_Surface* surf = SDL_CreateSurfaceFrom(x, y, fmt, data, pitch);
  if (!surf) Log::printf(LOG_ERROR, "surf == NULL %s", SDL_GetError());

  Log::printf(LOG_DEBUG, "Set icon to %s", str.c_str());
  SDL_SetWindowIcon(window, surf);
  SDL_DestroySurface(surf);

  if (imgData) free(data);
};

std::vector<gfx::BaseContext::DisplayMode>
SDLWindow::getSupportedDisplayModes() {
  std::vector<gfx::BaseContext::DisplayMode> dModes;
  int numDisplays;
  SDL_DisplayID* displays = SDL_GetDisplays(&numDisplays);
  for (int displayIdx = 0; displayIdx < numDisplays; displayIdx++) {
    int numModes;
    SDL_DisplayMode** modes =
        SDL_GetFullscreenDisplayModes(displayIdx, &numModes);
    for (int modeIdx = 0; modeIdx < numModes; modeIdx++) {
      SDL_DisplayMode dpmMode = *modes[modeIdx];
      gfx::BaseContext::DisplayMode mode;
      mode.w = dpmMode.w;
      mode.h = dpmMode.h;
      mode.display = displayIdx;
      mode.refresh_rate = dpmMode.refresh_rate;
      dModes.push_back(mode);
    }
  }
  SDL_free(displays);
  return dModes;
}

void SDLWindow::showMessageBox(MessageBoxType type, std::string title,
                               std::string message) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(),
                           message.c_str(), window);
}

glm::ivec2 SDLWindow::getWindowSize() {
  glm::ivec2 i;
  SDL_GetWindowSize(window, &i.x, &i.y);
  return i;
}
};  // namespace rdm

// little buggy fix because Xlib defines KeyPress for some reason and it
// conflicts with the rest of RDM

#ifdef __linux
#include <X11/Xlib.h>
#endif

namespace rdm {
void* SDLWindow::getGfxHwnd() {
#if defined(SDL_PLATFORM_LINUX)
  if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
    Display* xdisplay = (Display*)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER,
        NULL);
    Window xwindow = (Window)SDL_GetNumberProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
    return (void*)xwindow;
  } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
  }
#endif
  return NULL;
}
};  // namespace rdm
