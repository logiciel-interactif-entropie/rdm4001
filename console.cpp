#include "console.hpp"

#include <chrono>
#include <map>

#include "SDL_keycode.h"
#include "game.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/gui/font.hpp"
#include "gfx/gui/ngui.hpp"
#include "gfx/gui/ngui_elements.hpp"
#include "gfx/gui/ngui_window.hpp"
#include "input.hpp"
#include "logging.hpp"
#include "resource.hpp"
#include "settings.hpp"

#define CONSOLE_RATIO (179.f / 640.f)

namespace rdm {
struct ConCommandInfo {
  std::string usage;
  std::string description;
  ConsoleCommandFunction function;
};

struct ConsoleData {
  std::map<std::string, ConCommandInfo> consoleCommands;
};
static ConsoleData* ds = NULL;
ConsoleData* getCData() {
  if (!ds) ds = new ConsoleData;
  return ds;
}

#define CONSOLE_FONT "engine/gui/monospace.ttf"
#define CONSOLE_SIZE 18
class ConsoleWindow : public gfx::gui::NGuiWindow {
  gfx::gui::TextLabel* consoleLog;
  gfx::gui::TextInput* consoleLine;

 public:
  ConsoleWindow(gfx::gui::NGuiManager* gui, gfx::Engine* engine)
      : gfx::gui::NGuiWindow(gui, engine) {
    setTitle("Developer console");
    consoleLog = new gfx::gui::TextLabel(gui);
    consoleLog->setFont(gui->getFontCache()->get(CONSOLE_FONT, CONSOLE_SIZE));
    consoleLog->setTextMaxWidth(500);
    addElement(consoleLog);

    gfx::gui::NGuiPanel* lineInputPanel = new gfx::gui::NGuiPanel(gui);
    lineInputPanel->setLayout(new gfx::gui::NGuiHorizontalLayout());
    addElement(lineInputPanel);

    consoleLine = new gfx::gui::TextInput(gui);
    consoleLine->setFont(gui->getFontCache()->get(CONSOLE_FONT, CONSOLE_SIZE));
    consoleLine->setPrefix("] ");
    lineInputPanel->addElement(consoleLine);

    gfx::gui::Button* button0 = new gfx::gui::Button(gui);
    button0->setText("Run");
    button0->setPressed(std::bind(&ConsoleWindow::buttonPressed, this));
    lineInputPanel->addElement(button0);
  }

  virtual void frame() {
    const std::deque<LogMessage>& log = Log::singleton()->getLogMessages();
    std::string logText = "";
    for (int i = 20; i > 0; i--) {
      const LogMessage& m = log[i];
      if (m.t < Settings::singleton()->getCvar("cl_loglevel")->getInt()) {
        continue;
      }
      logText += m.message + "\n";
    }
    consoleLog->setText(logText);
  }

  void buttonPressed() {
    std::string cmd = consoleLine->getLine();
    getGame()->getConsole()->command(cmd);
    consoleLine->setText("");
  }
};
NGUI_INSTANTIATOR(ConsoleWindow);

Console::Console(Game* game) {
  this->game = game;
  visible = false;

  if (game->getWorld()) {
    game->getGfxEngine()->initialized.listen([this] {
      copyrightTexture =
          this->game->getGfxEngine()->getDevice()->createTexture();

      gfx::gui::OutFontTexture t = gfx::gui::FontRender::render(
          this->game->getGfxEngine()->getGuiManager()->getFontCache()->get(
              CONSOLE_FONT, CONSOLE_SIZE),
          "(c) logiciel interactif entropie 2024-2026");
      copyrightWidth = t.w;
      copyrightHeight = t.h;
      copyrightTexture->upload2d(t.w, t.h, gfx::DtUnsignedByte,
                                 gfx::BaseTexture::RGBA, t.data);
    });
    game->getWorld()->stepped.listen([this] { tick(); });

    bgTexture = game->getResourceManager()->load<resource::Texture>(
        "engine/gui/console_bg.png");
    textTexture = game->getGfxEngine()->getDevice()->createTexture();
#ifdef NDEBUG
    visible = false;
#else
    visible = true;
#endif
  } else {
  }
}

void Console::tick() {
  static bool debounce = false;
  if (Input::singleton()->isKeyDown(SDLK_BACKQUOTE) && !debounce) {
    debounce = true;
    game->getGfxEngine()->getGuiManager()->getGui<ConsoleWindow>()->open();
  } else if (!Input::singleton()->isKeyDown(SDLK_BACKQUOTE)) {
    debounce = false;
  }

  /* if (visible) {
    std::string& inp = Input::singleton()->getEditedText();
    if (!Input::singleton()->isEditingText()) {
      if (inp.empty()) {
        if (Input::singleton()->isKeyDown(SDLK_BACKSLASH)) {
          Input::singleton()->startEditingText();
        }
      } else {
        try {
          if (!inp.empty()) Log::printf(LOG_INFO, "] %s", inp.c_str());
          command(inp);
        } catch (std::exception& e) {
          Log::printf(LOG_ERROR, "Command error %s", e.what());
        }
        Input::singleton()->startEditingText();
      }
    }
  }*/
}

void Console::command(std::string in) {
  if (in.empty()) return;

  ConsoleArgReader r(in);
  std::string cmd = r.next();

  auto it = getCData()->consoleCommands.find(cmd);
  if (it != getCData()->consoleCommands.end()) {
    it->second.function(game, r);
  } else {
    CVar* cvar = Settings::singleton()->getCvar(cmd.c_str());
    if (cvar) {
      std::string rest = r.rest();
      if (rest.empty())
        Log::printf(LOG_INFO, "= \"%s\"", cvar->getValue().c_str());
      else
        cvar->setValue(r.rest());
    } else {
      Log::printf(LOG_ERROR, "Unknown cvar or command %s", in.c_str());
    }
  }
}

ConsoleArgReader::ConsoleArgReader(std::string in) { this->in = in; }

std::string ConsoleArgReader::next() {
  size_t lp = in.find(' ');
  std::string v = in.substr(0, lp);
  if (lp == std::string::npos) {
    in = "";
  } else {
    in = in.substr(lp + 1);
  }
  return v;
}

std::string ConsoleArgReader::rest() { return in; }

ConsoleCommand::ConsoleCommand(const char* name, const char* usage,
                               const char* description,
                               ConsoleCommandFunction func) {
  ConCommandInfo info;
  info.usage = usage;
  info.function = func;
  info.description = description;
  getCData()->consoleCommands[name] = info;
}

static ConsoleCommand unset("unset", "unset [cvar]", "sets cvar to blank",
                            [](Game* g, ConsoleArgReader r) {
                              std::string cvarName = r.next();
                              CVar* var = Settings::singleton()->getCvar(
                                  cvarName.c_str());
                              if (var) {
                                var->setValue("");
                              } else {
                                throw std::runtime_error(
                                    "No such cvar can be found");
                              }
                            });

static ConsoleCommand echo("echo", "echo [text]", "echos text to console log",
                           [](Game* g, ConsoleArgReader r) {
                             Log::printf(LOG_INFO, "%s", r.rest().c_str());
                           });

static ConsoleCommand cvars("cvars", "cvars", "lists all cvars",
                            [](Game* g, ConsoleArgReader r) {
                              Settings::singleton()->listCvars();
                            });

static ConsoleCommand help("help", "help", "lists all commands",
                           [](Game* game, ConsoleArgReader r) {
                             for (auto command : getCData()->consoleCommands) {
                               Log::printf(LOG_INFO, "%s - '%s'",
                                           command.second.usage.c_str(),
                                           command.second.description.c_str());
                             }
                           });

static ConsoleCommand exit("exit", "exit", "quits the game",
                           [](Game* game, ConsoleArgReader r) {
                             if (game->getWorld())
                               game->getWorld()->setRunning(false);
                             if (game->getServerWorld())
                               game->getServerWorld()->setRunning(false);
                           });

static ConsoleCommand connect(
    "connect", "connect [ip] [port]", "connects to server",
    [](Game* game, ConsoleArgReader r) {
      if (game->getWorld()) {
        if (!game->getWorldConstructorSettings().network)
          throw std::runtime_error("network disabled");

        std::string ip = r.next();
        int port = std::atoi(r.next().c_str());
        game->getWorld()->getNetworkManager()->connect(ip, port);
      } else {
        throw std::runtime_error("connect only works on client");
      }
    });

static ConsoleCommand disconnect(
    "disconnect", "disconnect", "disconnects from server",
    [](Game* game, ConsoleArgReader r) {
      if (game->getWorld()) {
        if (!game->getWorldConstructorSettings().network)
          throw std::runtime_error("network disabled");

        game->getWorld()->getNetworkManager()->requestDisconnect();
        if (game->getServerWorld()) {
          game->stopServer();
        }
      } else {
        throw std::runtime_error("disconnect only works on client");
      }
    });

static ConsoleCommand host(
    "host", "host [port]", "hosts a server",
    [](Game* game, ConsoleArgReader r) {
      if (game->getWorld()) {
        if (!game->getWorldConstructorSettings().network)
          throw std::runtime_error("network disabled");
        if (game->getServerWorld())
          throw std::runtime_error("already hosting server");

        int port = std::atoi(r.next().c_str());
        game->lateInitServer();
        try {
          game->getServerWorld()->getNetworkManager()->start(port);
        } catch (std::exception& e) {
          Log::printf(LOG_ERROR, "Error starting server %s", e.what());
        }
        game->getWorld()->getNetworkManager()->connect("127.0.0.1", port);
      } else {
        throw std::runtime_error("host only works on client");
      }
    });

};  // namespace rdm
