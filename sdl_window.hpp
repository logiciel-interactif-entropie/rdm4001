#pragma once
#include <SDL3/SDL.h>

#include "gfx/base_context.hpp"
#include "window.hpp"
namespace rdm {
class SDLWindow : public AbstractionWindow {
  SDL_Window* window;

  bool ignoreNextMouseMoveEvent;

 public:
  SDLWindow(Game* game);
  virtual ~SDLWindow();

  virtual void* getGfxHwnd();
  virtual void eventLoop();
  virtual void setTitle(std::string title);
  virtual void updateIcon(std::string str);

  virtual void showMessageBox(MessageBoxType type, std::string title,
                              std::string message);

  virtual std::vector<gfx::BaseContext::DisplayMode> getSupportedDisplayModes();
  virtual glm::ivec2 getWindowSize();
};
};  // namespace rdm
