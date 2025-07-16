#include "game.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_video.h>
#include <readline/history.h>
#include <readline/readline.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "SDL_clipboard.h"
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_messagebox.h"
#include "SDL_stdinc.h"
#include "SDL_surface.h"
#include "defs.hpp"
#include "filesystem.hpp"
#include "fun.hpp"
#include "gfx/apis.hpp"
#include "gfx/gl_context.hpp"
#include "gfx/stb_image.h"
#include "input.hpp"
#include "logging.hpp"
#include "network/network.hpp"
#include "resource.hpp"
#include "scheduler.hpp"
#include "security.hpp"
#include "settings.hpp"

#ifdef __linux
#include <signal.h>
#endif

#include "gfx/imgui/backends/imgui_impl_sdl2.h"
#include "gfx/imgui/imgui.h"

namespace rdm {
static CVar cl_copyright("cl_copyright", "1", CVARF_SAVE | CVARF_GLOBAL);
static CVar cl_loglevel("cl_loglevel", "2", CVARF_SAVE | CVARF_GLOBAL);
static CVar cl_savedwindowsize("cl_savedwindowsize", "800 600",
                               CVARF_SAVE | CVARF_GLOBAL);
static CVar cl_savedwindowpos("cl_savedwindowpos", "-1 -1",
                              CVARF_SAVE | CVARF_GLOBAL);

Game::Game(bool silence) {
  if (!Fun::preFlightChecks()) abort();  // clearly not safe to run
  this->silence = silence;

  if (!silence) Log::printf(LOG_INFO, "Hello World!");

  ignoreNextMouseMoveEvent = false;

  network::NetworkManager::initialize();
  resourceManager.reset(new ResourceManager());
  securityManager.reset(new SecurityManager());

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

  iconImg = "dat0/icon.png";
  dirtyIcon = true;
}

Game::~Game() {
  network::NetworkManager::deinitialize();

  if (window) {
    SDL_DestroyWindow(window);
  }
}

size_t Game::getVersion() { return ENGINE_VERSION; }

const char* Game::copyright() {
  return R"a(RDM4001, a 3D game engine 
Copyright (C) 2024-2026 Logiciel Interactif Entropie

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.)a";
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

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
    Log::printf(LOG_FATAL, "Unable to init SDL (%s)", SDL_GetError());
    throw std::runtime_error("SDL init failed");
  }

  world.reset(new World(worldSettings));

  if (!gfx::ApiFactory::singleton()->valid(r_api.getValue().c_str())) {
    Log::printf(LOG_ERROR, "r_api %s is invalid, setting to default %s",
                r_api.getValue().c_str(),
                gfx::ApiFactory::singleton()->getDefault());
    r_api.setValue(gfx::ApiFactory::singleton()->getDefault());
  }
  gfx::ApiFactory::ApiReg reg =
      gfx::ApiFactory::singleton()->getFunctions(r_api.getValue().c_str());
  int flags = reg.prepareSdl();

  glm::vec2 wsize = cl_savedwindowsize.getVec2();
  glm::ivec2 wpos = glm::ivec2(SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  if (cl_savedwindowpos.getVec2() != glm::vec2(-1)) {
    wpos = cl_savedwindowpos.getVec2();
  }
  Log::printf(LOG_DEBUG, "saved pos: %ix%i", wpos.x, wpos.y);
  flags |= fullscreen.getBool() ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE;

  window =
      SDL_CreateWindow("RDM4001!!!", wpos.x, wpos.y, wsize.x, wsize.y, flags);
  SDL_SetWindowMinimumSize(window, 800, 600);

  updateIcon();

  if (!window) {
    Log::printf(LOG_FATAL, "Unable to create Window (%s)", SDL_GetError());
    throw std::runtime_error("SDL window couldn't be created");
  }

  soundManager.reset(new SoundManager(world.get()));

  gfxEngine.reset(new gfx::Engine(world.get(), (void*)window));
  ImGui::SetCurrentContext(ImGui::CreateContext());
  ImGui_ImplSDL2_InitForOpenGL(
      window, ((gfx::gl::GLContext*)gfxEngine->getContext())->getContext());
  ImGui::GetIO().DisplaySize = ImVec2(wsize.x, wsize.y);

  if (worldSettings.network) {
    world->getNetworkManager()->setGfxEngine(gfxEngine.get());
    world->getNetworkManager()->setGame(this);
  }
  world->changingTitle.listen([this](std::string title) {
#ifndef _WIN32
    SDL_SetWindowTitle(window, title.c_str());
#endif
  });

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
  auto imgData = common::FileSystem::singleton()->readFile(iconImg.c_str());
  if (imgData) {
    int x, y, ch;
    stbi_uc* uc =
        stbi_load_from_memory(imgData->data(), imgData->size(), &x, &y, &ch, 4);
    int pitch = ((x * ch) + 3) & ~3;
    int rmask, gmask, bmask, amask;

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = (ch == 4) ? 0xff000000 : 0;
#else
    int s = (ch == 4) ? 0 : 8;
    rmask = 0xff000000 >> s;
    gmask = 0x00ff0000 >> s;
    bmask = 0x0000ff00 >> s;
    amask = 0x000000ff >> s;
#endif

    SDL_Surface* surf = SDL_CreateRGBSurfaceFrom(uc, x, y, ch * 8, pitch, rmask,
                                                 gmask, bmask, amask);
    Log::printf(LOG_DEBUG, "Set icon to %s", iconImg.c_str());
    SDL_SetWindowIcon(window, surf);
    SDL_FreeSurface(surf);
    free(uc);
  }

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
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error in initialization",
                               e.what(), window);
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

  Input::singleton()->beginFrame();
  SDL_Event event;

#ifndef _WIN32
  if (dirtyIcon) updateIcon();
#endif

  bool ignoreMouse = false;
  SDL_ShowCursor(!Input::singleton()->getMouseLocked());
  while (SDL_PollEvent(&event)) {
    if (input_enableimgui.getBool()) {
      std::scoped_lock l(gfxEngine->getImguiLock());
      ImGui_ImplSDL2_NewFrame();
      ImGui_ImplSDL2_ProcessEvent(&event);
    }
    InputObject object;

    switch (event.type) {
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_RESIZED:
            cl_savedwindowsize.setVec2(
                glm::vec2(event.window.data1, event.window.data2));
            ImGui::GetIO().DisplaySize =
                ImVec2(event.window.data1, event.window.data2);
            break;
          case SDL_WINDOWEVENT_MOVED:
            cl_savedwindowpos.setVec2(
                glm::vec2(event.window.data1, event.window.data2));
            break;
          default:
            break;
        }
        break;
      case SDL_TEXTINPUT:
        if (!(SDL_GetModState() & KMOD_CTRL &&
              (event.text.text[0] == 'c' || event.text.text[0] == 'C' ||
               event.text.text[0] == 'v' || event.text.text[0] == 'V'))) {
          std::string& text = Input::singleton()->getEditedText();
          text += event.text.text;
        }
        break;
      case SDL_MOUSEBUTTONUP:
      case SDL_MOUSEBUTTONDOWN:
        object.type = event.type == SDL_MOUSEBUTTONDOWN
                          ? InputObject::MousePress
                          : InputObject::MouseUp;
        object.data.mouse.mouse_down = object.type == InputObject::MousePress;
        object.data.mouse.button = event.button.button;
        Input::singleton()->postEvent(object);
        break;
      case SDL_KEYUP:
      case SDL_KEYDOWN:
        if (Input::singleton()->isEditingText()) {
          if (event.type != SDL_KEYDOWN) break;
          std::string& text = Input::singleton()->getEditedText();
          if (event.key.keysym.sym == SDLK_BACKSPACE && text.length() != 0) {
            text.pop_back();
          } else if (event.key.keysym.sym == SDLK_c &&
                     SDL_GetModState() & KMOD_CTRL) {
            SDL_SetClipboardText(text.c_str());
          } else if (event.key.keysym.sym == SDLK_v &&
                     SDL_GetModState() & KMOD_CTRL) {
            char* temp = SDL_GetClipboardText();
            text = temp;
            SDL_free(temp);
          } else if (event.key.keysym.sym == SDLK_RETURN) {
            Input::singleton()->stopEditingText();
          }
        } else {
          object.type = event.type == SDL_KEYDOWN ? InputObject::KeyPress
                                                  : InputObject::KeyUp;
          object.data.key.key = event.key.keysym.sym;
          Input::singleton()->postEvent(object);
        }
        break;
      case SDL_QUIT:
        object.type = InputObject::Quit;
        Input::singleton()->postEvent(object);
        break;
      case SDL_MOUSEMOTION:
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
          if (input_userelativemode.getBool()) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            object.data.mouse.position[0] = w / 2;
            object.data.mouse.position[1] = h / 2;
          } else {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_WarpMouseInWindow(window, w / 2, h / 2);
            SDL_ShowCursor(!Input::singleton()->getMouseLocked());
            object.data.mouse.position[0] = w / 2;
            object.data.mouse.position[1] = h / 2;
            ignoreNextMouseMoveEvent = true;
          }
        } else {
          if (input_userelativemode.getBool())
            SDL_SetRelativeMouseMode(SDL_FALSE);
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
}
}  // namespace rdm
