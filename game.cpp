#include "game.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_video.h>
#include <readline/history.h>
#include <readline/readline.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <stdexcept>

#include "SDL3/SDL_clipboard.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"
#include "defs.hpp"
#include "filesystem.hpp"
#include "fun.hpp"
#include "gfx/apis.hpp"
#include "gfx/gl_context.hpp"
#include "http.hpp"
#include "input.hpp"
#include "localization.hpp"
#include "logging.hpp"
#include "network/network.hpp"
#include "object.hpp"
#include "object_property.hpp"
#include "pak_file.hpp"
#include "resource.hpp"
#include "scheduler.hpp"
#include "script/script_api.hpp"
#include "script/script_context.hpp"
#include "security.hpp"
#include "settings.hpp"
#include "window.hpp"
#include "worker.hpp"
#include "world.hpp"

#ifdef RDM4001_FEATURE_SDL
#include "sdl_window.hpp"
#endif

#ifdef __linux
#include <signal.h>
#endif

#include "gfx/imgui/backends/imgui_impl_sdl3.h"
#include "gfx/imgui/imgui.h"

namespace rdm {
static CVar cl_copyright("cl_copyright", "1", CVARF_SAVE | CVARF_GLOBAL);
static CVar cl_loglevel("cl_loglevel", "2", CVARF_SAVE | CVARF_GLOBAL);
static CVar cl_savedwindowsize("cl_savedwindowsize", "800 600",
                               CVARF_SAVE | CVARF_GLOBAL);
static CVar cl_savedwindowpos("cl_savedwindowpos", "-1 -1",
                              CVARF_SAVE | CVARF_GLOBAL);

static int __game_StartServer(lua_State* L) {
  Game* game = dynamic_cast<Game*>(script::ObjectBridge::getDescribed(L, 1));
  if (!game) throw std::runtime_error("game == NULL");
  if (game->getServerWorld())
    throw std::runtime_error("Server world already started");
  game->startServer();

  script::ObjectBridge::pushDescribed(L, game->getServerWorld());
  return LUA_OK;
}

RDM_REFLECTION_BEGIN_DESCRIBED(Game);
RDM_REFLECTION_PROPERTY_OBJECT(Game, World, &Game::getWorld, NULL);
RDM_REFLECTION_PROPERTY_OBJECT(Game, WorldServer, &Game::getServerWorld, NULL);
RDM_REFLECTION_PROPERTY_OBJECT(Game, GfxEngine, &Game::getGfxEngine, NULL);
RDM_REFLECTION_PROPERTY_INT(Game, EngineVersion, &Game::getVersion, NULL);
RDM_REFLECTION_PROPERTY_INT(Game, GameVersion, &Game::getGameVersion, NULL);
RDM_REFLECTION_PROPERTY_STRING(Game, Icon, &Game::getIcon, &Game::setIcon);
RDM_REFLECTION_PROPERTY_FUNCTION(Game, StartServer, &__game_StartServer);
RDM_REFLECTION_END_DESCRIBED();

Game::Game(bool silence) {
  if (!Fun::preFlightChecks()) abort();  // clearly not safe to run
  this->silence = silence;

  WorkerManager::singleton();  // just make sure workermanager is running

  if (!silence) Log::printf(LOG_INFO, Lc(RDM_HELLO_WORLD, "Hello World!"));

  network::NetworkManager::initialize();
  resourceManager.reset(new ResourceManager());
  securityManager.reset(new SecurityManager());

  reflection::Reflection::singleton()->execPrecacheFunctions(
      resourceManager.get());

  initialized = false;
  window = NULL;
  worldSettings.game = this;

  Log::singleton()->setLevel((LogType)cl_loglevel.getInt());
  cl_loglevel.changing.listen(
      [] { Log::singleton()->setLevel((LogType)cl_loglevel.getInt()); });

  if (!silence) {
    Log::printf(LOG_INFO, "RDM engine version %06x", ENGINE_VERSION);
    Log::printf(LOG_INFO, "RDM protocol version %06x", PROTOCOL_VERSION);
    if (cl_copyright.getBool()) Log::printf(LOG_INFO, "%s", copyright());
  }

  iconImg = "engine/assets/icon.png";
  dirtyIcon = true;
}

Game::~Game() { network::NetworkManager::deinitialize(); }

size_t Game::getVersion() { return ENGINE_VERSION; }

const char* Game::copyright() {
  return Lc(RDM_LONG_COPYRIGHT, R"a(See our epic license below!
RDM4001, a 3D game engine 
Copyright (C) 2024-2026 logiciel interactif Entropie

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
)a");
}

void Game::startGameState(GameStateConstructorFunction f) {
  if (!world) {
    throw std::runtime_error(
        "Game::startGameState called before Game::startClient");
  }
  gameState.reset(f(this));
}

static CVar fullscreen("fullscreen", "0", CVARF_SAVE | CVARF_GLOBAL);
static CVar r_api("r_api", "GLModern", CVARF_SAVE | CVARF_GLOBAL);

void Game::startClient() {
  if (!gfx::ApiFactory::singleton()->platformSupportsGraphics())
    throw std::runtime_error("This build does not contain any graphics apis");

  world.reset(new World(worldSettings));

  if (!gfx::ApiFactory::singleton()->valid(r_api.getValue().c_str())) {
    Log::printf(LOG_ERROR, "r_api %s is invalid, setting to default %s",
                r_api.getValue().c_str(),
                gfx::ApiFactory::singleton()->getDefault());
    r_api.setValue(gfx::ApiFactory::singleton()->getDefault());
  }
  gfx::ApiFactory::ApiReg reg =
      gfx::ApiFactory::singleton()->getFunctions(r_api.getValue().c_str());

  glm::vec2 wsize = cl_savedwindowsize.getVec2();
  glm::ivec2 wpos = glm::ivec2(SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  if (cl_savedwindowpos.getVec2() != glm::vec2(-1)) {
    wpos = cl_savedwindowpos.getVec2();
  }
  Log::printf(LOG_DEBUG, "saved pos: %ix%i", wpos.x, wpos.y);

#ifdef RDM4001_FEATURE_SDL
  window.reset(new SDLWindow(this));
#endif

  updateIcon();

  soundManager.reset(new SoundManager(world.get()));

  gfxEngine.reset(new gfx::Engine(world.get(), window.get()));
  // ImGui::SetCurrentContext(ImGui::CreateContext());
  // ImGui_ImplSDL3_InitForOpenGL(
  //     window, ((gfx::gl::GLContext*)gfxEngine->getContext())->getContext());
  // ImGui::GetIO().DisplaySize = ImVec2(wsize.x, wsize.y);

  if (worldSettings.network) {
    world->getNetworkManager()->setGfxEngine(gfxEngine.get());
    world->getNetworkManager()->setGame(this);
  }
  world->changingTitle.listen(
      [this](std::string title) { window->setTitle(title.c_str()); });

  console.reset(new Console(this));

  gfxEngine->getContext()->unsetCurrent();
}

void Game::startServer() {
  if (!worldSettings.network)
    Log::printf(LOG_WARN,
                "startServer called while worldSettings.network = false");

  worldServer.reset(new World(worldSettings));
  if (worldSettings.network) worldServer->getNetworkManager()->setGame(this);
  if (!console) console.reset(new Console(this));
}

void Game::lateInitServer() {
  Log::printf(LOG_INFO, "Starting built-in server");
  startServer();
  initializeServer();
  worldServer->getScheduler()->startAllJobs();
}

static CVar input_rate("input_rate", "60.0", CVARF_SAVE | CVARF_GLOBAL);
static CVar sv_ansi("sv_ansi", "1", CVARF_SAVE | CVARF_GLOBAL);

class GameEventJob : public SchedulerJob {
  Game* game;

 public:
  GameEventJob(Game* game) : SchedulerJob("GameEvent") { this->game = game; }

  virtual Result step() {
    game->pollEvents();
    return Stepped;
  }

  virtual double getFrameRate() { return 1.0 / input_rate.getFloat(); }
};

class ConsoleLineInputJob : public SchedulerJob {
  Console* console;

 public:
  ConsoleLineInputJob(Console* console) : SchedulerJob("ConsoleLineInput") {
    Log::printf(LOG_WARN,
                "You have to send ^C or enter to the line buffer when exiting, "
                "or it will wait on the job");  // FIXME

    this->console = console;
  }

  virtual Result step() {
    char* line = readline("] ");
    if (line && *line) {
      try {
        this->console->command(line);
      } catch (std::exception& e) {
        Log::printf(LOG_ERROR, "Command error %s", e.what());
      }
      add_history(line);
    }
    return Stepped;
  }

  virtual double getFrameRate() { return 0.0; }
};

class ResourceManagerTickJob : public SchedulerJob {
  Game* game;

 public:
  ResourceManagerTickJob(Game* game) : SchedulerJob("ResourceManagerTick") {
    this->game = game;
  }

  virtual Result step() {
    game->getResourceManager()->tick();
    return Stepped;
  }
};

static CVar enable_console("enable_console", "1", CVARF_SAVE | CVARF_GLOBAL);

void Game::updateIcon() {
  window->updateIcon(iconImg.c_str());
  dirtyIcon = false;
}

void Game::earlyInit() {
  try {
    initialize();
    bool createdTickJob = false;

    if (world) {
      initializeClient();

      world->getScheduler()->addJob(new GameEventJob(this));
      world->getScheduler()->addJob(new ResourceManagerTickJob(this));
      createdTickJob = true;
      world->getScheduler()->startAllJobs();
    }
    if (worldServer) {
      initializeServer();
      if (sv_ansi.getBool()) {
        worldServer->changingTitle.listen([](std::string title) {
          fprintf(stderr, "\033]0;%s\007", title.c_str());
        });
      }
      if (!createdTickJob) {
        worldServer->getScheduler()->addJob(new ResourceManagerTickJob(this));
        if (enable_console.getBool())
          worldServer->getScheduler()->addJob(
              new ConsoleLineInputJob(console.get()));
      }
      worldServer->getScheduler()->startAllJobs();
    }

    initialized = true;
  } catch (std::exception& e) {
    Log::printf(LOG_FATAL, "Error initializing game: %s", e.what());
    if (world) {
      window->showMessageBox(AbstractionWindow::Error, "ERROR", e.what());
    }
    exit(EXIT_FAILURE);
  }
}

void Game::stopServer() { worldServer.reset(); }

#ifdef NDEBUG
static CVar input_userelativemode("input_userelativemode", "1",
                                  CVARF_SAVE | CVARF_GLOBAL);
#else
static CVar input_userelativemode("input_userelativemode", "0",
                                  CVARF_SAVE | CVARF_GLOBAL);

#endif
static CVar input_enableimgui("input_enableimgui", "1",
                              CVARF_SAVE | CVARF_GLOBAL);

void Game::pollEvents() {
  std::scoped_lock l(Input::singleton()->getFlushingMutex());

  Input::singleton()->setWindow(window.get());
  Input::singleton()->beginFrame();

#ifndef _WIN32
  if (dirtyIcon) updateIcon();
#endif

  window->eventLoop();
}

void Game::mainLoop() {
  if (!initialized) earlyInit();

  if (!world && !worldServer) {
    Log::printf(LOG_FATAL,
                "world or worldServer is not set, please call startClient "
                "or startServer in your Game::initialize function");
  }

  if (world) {
    while (world->getRunning()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  if (world) {
    world->getScheduler()->waitToWrapUp();
    if (worldSettings.network) world->getNetworkManager()->handleDisconnect();
  }
  if (worldServer) {
    worldServer->getScheduler()->waitToWrapUp();
    if (worldSettings.network)
      worldServer->getNetworkManager()->handleDisconnect();
  }

  Log::printf(LOG_DEBUG, "World no longer running");
  if (world) {
    gfxEngine->getContext()->setCurrent();
  }

  gameState.reset();  // needs to be deinitialized before workermanager cause
                      // gamestate might want to run web requests before dying
  WorkerManager::singleton()->shutdown();
}
}  // namespace rdm
