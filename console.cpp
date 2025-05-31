#include "console.hpp"

#include <chrono>
#include <map>

#include "SDL_keycode.h"
#include "game.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/gui/font.hpp"
#include "gfx/gui/gui.hpp"
#include "input.hpp"
#include "logging.hpp"
#include "resource.hpp"
#include "settings.hpp"
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

#define CONSOLE_FONT "dat3/monospace.ttf"
#define CONSOLE_SIZE 18

Console::Console(Game* game) {
  this->game = game;
  visible = false;

  if (game->getWorld()) {
    game->getGfxEngine()->afterGuiRenderStepped.listen([this] { render(); });
    game->getGfxEngine()->initialized.listen([this] {
      copyrightTexture =
          this->game->getGfxEngine()->getDevice()->createTexture();

      gfx::gui::OutFontTexture t = gfx::gui::FontRender::render(
          this->game->getGfxEngine()->getGuiManager()->getFontCache()->get(
              CONSOLE_FONT, CONSOLE_SIZE),
          "(c) entropy software 2024-2026, RDM4001 is licensed under the GNU "
          "GPLv3");
      copyrightWidth = t.w;
      copyrightHeight = t.h;
      copyrightTexture->upload2d(t.w, t.h, gfx::DtUnsignedByte,
                                 gfx::BaseTexture::RGBA, t.data);
    });
    game->getWorld()->stepped.listen([this] { tick(); });

    bgTexture = game->getResourceManager()->load<resource::Texture>(
        "dat3/console_bg.png");
    textTexture = game->getGfxEngine()->getDevice()->createTexture();
#ifdef NDEBUG
    visible = false;
#else
    visible = true;
    Input::singleton()->startEditingText();
#endif
  }
}

void Console::render() {
  using namespace std::chrono_literals;

  if (!visible) return;

  rdm::gfx::Engine* engine = game->getGfxEngine();
  gfx::gui::GuiManager* manager = engine->getGuiManager();
  gfx::gui::FontCache* fontcache = manager->getFontCache();
  std::chrono::time_point now = std::chrono::steady_clock::now();
  std::chrono::time_point d = now - 30s;
  const std::deque<LogMessage>& log = Log::singleton()->getLogMessages();
  std::vector<LogMessage> toLog;
  bool dirty = false;
  glm::vec2 tres = engine->getTargetResolution();
  for (int i = 0; i < std::min(log.size(), (size_t)(tres.y / CONSOLE_SIZE));
       i++) {
    const LogMessage& m = log[i];
    if (i == 0) {
      if (m.time != last_message) {
        dirty = true;
        last_message = m.time;
      }
    }

    if (m.time < d) continue;
    if (m.t < Settings::singleton()->getCvar("cl_loglevel")->getInt()) {
      continue;
    }

    toLog.push_back(m);
  }

  if (Input::singleton()->isEditingText()) {
    if (lastText != Input::singleton()->getEditedText()) {
      dirty = true;
      lastText = Input::singleton()->getEditedText();
    }
  }

  if (dirty) {
    std::string msg;
    for (int i = toLog.size() - 1; i >= 0; --i) {
      const LogMessage& m = toLog[i];
      msg += m.message + "\n";
    }

    if (Input::singleton()->isEditingText()) {
      msg += "] " + Input::singleton()->getEditedText() + "\n";
    }

    gfx::gui::OutFontTexture t = gfx::gui::FontRender::renderWrapped(
        fontcache->get(CONSOLE_FONT, CONSOLE_SIZE), msg.c_str(), tres.x);
    textTexture->upload2d(t.w, t.h, gfx::DtUnsignedByte, gfx::BaseTexture::RGBA,
                          t.data);
    consoleWidth = t.w;
    consoleHeight = t.h;
  }

  engine->getDevice()->setBlendState(gfx::BaseDevice::SrcAlpha,
                                     gfx::BaseDevice::OneMinusSrcAlpha);

  gfx::BaseProgram* bp =
      manager->getImageMaterial()->prepareDevice(engine->getDevice(), 0);

  bp->setParameter(
      "scale", gfx::DtVec2,
      gfx::BaseProgram::Parameter{.vec2 = glm::vec2(tres.x, tres.y / 2)});
  bp->setParameter(
      "texture0", gfx::DtSampler,
      gfx::BaseProgram::Parameter{.texture.slot = 0,
                                  .texture.texture = bgTexture->getTexture()});
  bp->setParameter("color", gfx::DtVec3,
                   gfx::BaseProgram::Parameter{.vec3 = glm::vec3(1, 1, 1)});
  bp->setParameter(
      "offset", gfx::DtVec2,
      gfx::BaseProgram::Parameter{.vec2 = glm::vec2(0, tres.y / 2)});
  bp->bind();
  manager->getSArrayPointers()->bind();
  engine->getDevice()->draw(manager->getSElementBuf(), gfx::DtUnsignedByte,
                            gfx::BaseDevice::Triangles, 6);

  bp->setParameter(
      "texture0", gfx::DtSampler,
      gfx::BaseProgram::Parameter{.texture.slot = 0,
                                  .texture.texture = textTexture.get()});
  bp->setParameter("scale", gfx::DtVec2,
                   gfx::BaseProgram::Parameter{
                       .vec2 = glm::vec2(consoleWidth, consoleHeight)});
  bp->bind();
  engine->getDevice()->draw(manager->getSElementBuf(), gfx::DtUnsignedByte,
                            gfx::BaseDevice::Triangles, 6);

  bp->setParameter(
      "offset", gfx::DtVec2,
      gfx::BaseProgram::Parameter{.vec2 = glm::vec2(tres.x - copyrightWidth,
                                                    tres.y - copyrightHeight)});
  bp->setParameter(
      "texture0", gfx::DtSampler,
      gfx::BaseProgram::Parameter{.texture.slot = 0,
                                  .texture.texture = copyrightTexture.get()});
  bp->setParameter("scale", gfx::DtVec2,
                   gfx::BaseProgram::Parameter{
                       .vec2 = glm::vec2(copyrightWidth, copyrightHeight)});
  bp->setParameter(
      "color", gfx::DtVec3,
      gfx::BaseProgram::Parameter{.vec3 = glm::vec3(0.92, 0.67, 0.0)});
  bp->bind();
  engine->getDevice()->draw(manager->getSElementBuf(), gfx::DtUnsignedByte,
                            gfx::BaseDevice::Triangles, 6);
}

void Console::tick() {
  static bool debounce = false;
  if (Input::singleton()->isKeyDown(SDLK_BACKQUOTE) && !debounce) {
    debounce = true;
    if (visible) {
      visible = false;
      Input::singleton()->stopEditingText();
    } else {
      visible = true;
      Input::singleton()->startEditingText();
    }
  } else if (!Input::singleton()->isKeyDown(SDLK_BACKQUOTE)) {
    debounce = false;
  }

  if (visible) {
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
  }
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
        Log::printf(LOG_INFO, "= %s", cvar->getValue().c_str());
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
