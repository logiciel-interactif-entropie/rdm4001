#pragma once
#include <chrono>
#include <functional>
#include <string>

#include "gfx/base_types.hpp"
#include "gfx/gui/gui.hpp"
#include "resource.hpp"

namespace rdm {
class Game;
class Console {
  friend class ConsoleCommand;

  std::chrono::time_point<std::chrono::steady_clock> last_message;
  std::unique_ptr<gfx::BaseTexture> textTexture;
  int consoleHeight;
  int consoleWidth;

  std::unique_ptr<gfx::BaseTexture> copyrightTexture;
  int copyrightHeight;
  int copyrightWidth;

  bool inputCommand;
  bool visible;
  std::string lastText;

  resource::Texture* bgTexture;

  Game* game;

  void render();
  void tick();

 public:
  Console(Game* game);

  void command(std::string in);
};

class ConsoleArgReader {
  std::string in;

 public:
  ConsoleArgReader(std::string in);

  std::string next();
  std::string rest();
};

typedef std::function<void(Game*, ConsoleArgReader)> ConsoleCommandFunction;

class ConsoleCommand {
 public:
  ConsoleCommand(const char* name, const char* usage, const char* description,
                 ConsoleCommandFunction function);
};
}  // namespace rdm
