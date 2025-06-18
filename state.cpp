#include "state.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <numeric>

#include "fun.hpp"
#include "game.hpp"
#include "gfx/base_types.hpp"
#include "gfx/gui/gui.hpp"
#include "input.hpp"
#include "network/network.hpp"
#include "script/my_basic.h"
#include "settings.hpp"
namespace rdm {
static CVar cl_nointro("cl_nointro", "0", CVARF_GLOBAL | CVARF_SAVE);

static int _BasGetState(mb_interpreter_t* s, void** l) {
  std::map<GameState::States, std::string> stateNames = {
      {GameState::MainMenu, "MainMenu"},
      {GameState::MenuOnlinePlay, "MenuOnlinePlay"},
      {GameState::InGame, "InGame"},
      {GameState::Intro, "Intro"},
      {GameState::Connecting, "Connecting"},
  };

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_attempt_close_bracket(s, l));

  script::Context* context;
  mb_get_userdata(s, (void**)&context);

  std::string stateName =
      stateNames[context->getWorld()->getGame()->getGameState()->getState()];

  mb_push_string(s, l, mb_memdup(stateName.data(), stateName.size() + 1));

  return MB_FUNC_OK;
}

static int _BasSetState(mb_interpreter_t* s, void** l) {
  char* state;

  std::map<std::string, GameState::States> stateNames = {
      {"MainMenu", GameState::MainMenu},
      {"MenuOnlinePlay", GameState::MenuOnlinePlay},
      {"InGame", GameState::InGame},
      {"Intro", GameState::Intro},
      {"Connecting", GameState::Connecting}};

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &state));
  mb_check(mb_attempt_close_bracket(s, l));

  auto it = stateNames.find(state);
  if (it == stateNames.end()) {
    Log::printf(LOG_ERROR, "Unknown state %s", state);
    return MB_FUNC_ERR;
  }

  script::Context* context;
  mb_get_userdata(s, (void**)&context);
  context->getWorld()->getGame()->getGameState()->setState(stateNames[state]);

  return MB_FUNC_OK;
}

static int _BasStateCallback(mb_interpreter_t* s, void** l) {
  char* runtime;

  mb_check(mb_attempt_open_bracket(s, l));
  mb_check(mb_pop_string(s, l, &runtime));
  mb_check(mb_attempt_close_bracket(s, l));

  script::Context* context;
  mb_get_userdata(s, (void**)&context);

  std::string _runtime = runtime;
  std::transform(_runtime.begin(), _runtime.end(), _runtime.begin(), ::toupper);
  context->getWorld()->getGame()->getGameState()->switchingState.listen(
      [s, l, _runtime] {
        mb_value_t routine;
        mb_value_t args[1];
        mb_value_t ret;
        mb_get_routine(s, l, _runtime.c_str(), &routine);
        mb_make_nil(ret);
        mb_make_nil(args[0]);
        mb_eval_routine(s, l, routine, args, 0, &ret);
      });

  return MB_FUNC_OK;
}

GameState::GameState(Game* game) {
  this->game = game;
  state = cl_nointro.getBool() ? MainMenu : Intro;
  timer = 10.f;
  emitter = game->getSoundManager()->newEmitter();
  entropyLogo =
      game->getResourceManager()->load<resource::Model>("dat0/entropy.obj");

  game->getWorld()->getScriptContext()->setContextCall.listen(
      [](mb_interpreter_t* s) {
        mb_begin_module(s, "STATE");
        mb_register_func(s, "SET", _BasSetState);
        mb_register_func(s, "GET", _BasGetState);
        mb_register_func(s, "ADDCALLBACK", _BasStateCallback);
        mb_end_module(s);
      });

  game->getGfxEngine()->initialized.listen([this, game] {});

  game->getGfxEngine()->renderStepped.listen([this, game] {
    switch (state) {
      case Connecting:
      default:
      case MainMenu: {
        game->getGfxEngine()->setClearColor(glm::vec3(1.f));
      } break;
      case Intro: {
        Graph::Node node;
        node.basis = glm::identity<glm::mat3>();
        node.origin = glm::vec3(0);
        gfx::Camera& camera =
            game->getGfxEngine()->getCurrentViewport()->getCamera();
        float time = game->getGfxEngine()->getTime();
        game->getGfxEngine()->setClearColor(glm::vec3(0.0, 0.0, 0.0));
        if (time < 1.0) {
          camera.setTarget(
              glm::vec3(2.f - (time * 2.f), 0.0, 2.f - (time * 2.f)));
        } else if (time > 9.0) {
          camera.setTarget(
              glm::vec3(0.0, glm::mix(0.0, 10.0, 9.f - time), 0.0));
          game->getGfxEngine()->setClearColor(
              glm::mix(glm::vec3(0.0), glm::vec3(1), time - 9.f));
        } else {
        }
        camera.setUp(glm::vec3(0.0, 1.0, 0.0));
        camera.setNear(0.01f);
        camera.setFar(100.0f);
        camera.setFOV(30.f + (time * 4.f));
        float distance = 4.0 + time;
        camera.setPosition(glm::vec3(0.0, 0.0, distance));
        entropyLogo->render(game->getGfxEngine()->getDevice(), NULL, NULL,
                            [&node](gfx::BaseProgram* program) {
                              program->setParameter(
                                  "model", gfx::DtMat4,
                                  gfx::BaseProgram::Parameter{
                                      .matrix4x4 = node.worldTransform()});
                            });
      } break;
    }
  });

  game->getWorld()->stepped.listen([this, game] {
    if (!emitter->isPlaying()) {
      if (stateMusic.find(state) != stateMusic.end())
        emitter->play(game->getSoundManager()
                          ->getSoundCache()
                          ->get(stateMusic[state], Sound::Stream)
                          .value());
      else
        emitter->stop();
    }

    if (game->getWorld()->getNetworkManager()->getLocalPeer().type ==
        network::Peer::ConnectedPlayer) {
      setState(InGame);
    } else if (game->getWorld()->getNetworkManager()->getLocalPeer().type ==
               network::Peer::Undifferentiated) {
      setState(Connecting);
    }

    switch (state) {
      case Intro:
        timer -= 1.0 / 60.0;
        if (timer <= 0.0) {
          setState(MainMenu);
          emitter->stop();
        }
        break;
      case MainMenu: {
      } break;
      case Connecting:
      case InGame:
        if (game->getWorld()->getNetworkManager()->getLocalPeer().type ==
            network::Peer::Unconnected) {
          setState(MainMenu);
        }
        break;
      default:
        break;
    }
  });
}

}  // namespace rdm
