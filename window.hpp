#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "gfx/base_context.hpp"
namespace rdm {
class Game;
class AbstractionWindow {
  Game* game;

 public:
  AbstractionWindow(Game* game);
  virtual ~AbstractionWindow() = default;

  Game* getGame() { return game; }

  virtual void* getGfxHwnd() = 0;
  virtual void eventLoop() = 0;
  virtual void setTitle(std::string title) {};
  virtual void updateIcon(std::string str) {};

  enum MessageBoxType { Error, Warning, Info };

  virtual void showMessageBox(MessageBoxType type, std::string title,
                              std::string message) = 0;

  virtual glm::ivec2 getWindowSize() = 0;

  virtual std::vector<gfx::BaseContext::DisplayMode>
  getSupportedDisplayModes() = 0;
};
};  // namespace rdm
