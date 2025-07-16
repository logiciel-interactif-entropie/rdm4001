#include "state.hpp"

#include <glm/ext/matrix_transform.hpp>
#include <numeric>

#include "fun.hpp"
#include "game.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/gui/font.hpp"
#include "gfx/gui/ngui.hpp"
#include "gfx/gui/ngui_window.hpp"
#include "input.hpp"
#include "network/network.hpp"
#include "settings.hpp"
namespace rdm {
static CVar cl_nointro("cl_nointro", "0", CVARF_GLOBAL | CVARF_SAVE);

class ConnectionGui : public gfx::gui::NGuiWindow {
  char ipStr[64];
  char portStr[7];

 public:
  ConnectionGui(gfx::gui::NGuiManager* manager, gfx::Engine* engine)
      : gfx::gui::NGuiWindow(manager, engine) {
    setTitle("Connection");
    memset(ipStr, 0, sizeof(ipStr));
    memset(portStr, 0, sizeof(portStr));
  }

  virtual void closing() {
    getGame()->getGameState()->setState(GameState::MainMenu);
  }
};

class ConnectingGui : public gfx::gui::NGuiWindow {
 public:
  ConnectingGui(gfx::gui::NGuiManager* manager, gfx::Engine* engine)
      : gfx::gui::NGuiWindow(manager, engine) {
    setTitle("Connecting");
  }
};

class PlayGameGui : public gfx::gui::NGuiWindow {
 public:
  PlayGameGui(gfx::gui::NGuiManager* manager, gfx::Engine* engine)
      : gfx::gui::NGuiWindow(manager, engine) {
    setTitle("Play Game");
  }

  virtual void closing() {
    if (!getManager()->getGui<ConnectionGui>()->isVisible()) {
      getGame()->getGameState()->setState(GameState::MainMenu);
    }
  }
};

NGUI_INSTANTIATOR(ConnectingGui);
NGUI_INSTANTIATOR(ConnectionGui);
NGUI_INSTANTIATOR(PlayGameGui);

class MainMenuGui : public gfx::gui::NGui {
  gfx::gui::Font* font;

  void renderMainMenu(gfx::gui::NGuiRenderer* renderer) {
    glm::vec2 res = getEngine()->getTargetResolution();
    glm::vec2 spacePerElement = res;
    int elems = 0;
    struct Entry {
      std::string name;
      GameState::States state;
      gfx::gui::NGuiWindow* toOpen;
    };
    std::vector<Entry> states;
    states.push_back((Entry){"Online Play", GameState::MenuOnlinePlay,
                             getManager()->getGui<PlayGameGui>()});
    states.push_back((Entry){"Settings", GameState::Todo, NULL});
    states.push_back((Entry){"Quit", GameState::Todo, NULL});

    spacePerElement.x /= states.size();

    for (auto& state : states) {
      switch (renderer->mouseDownZone(glm::vec2(spacePerElement.x * elems, 0),
                                      glm::vec2(spacePerElement.x, 32.f))) {
        default:
        case -1:
          renderer->setColor(glm::vec3(1));
          break;
        case 0:
          renderer->setColor(glm::vec3(0.5));
          break;
        case 1:
          renderer->setColor(glm::vec3(0.2));
          getGame()->getGameState()->setState(state.state);
          if (state.toOpen) state.toOpen->open();
          break;
      }
      renderer->text(glm::ivec2(0, 0), font, 0, "%s", state.name.c_str());
      glm::vec2 offset = glm::vec2(
          (spacePerElement.x * elems++) + (spacePerElement.x / 2.f) -
              (renderer->getLastCommand()->getScale().value().x / 2.f),
          0);
      renderer->getLastCommand()->setOffset(offset);
    }
  }

  void renderConnecting(gfx::gui::NGuiRenderer* renderer) {
    getManager()->getGui<ConnectingGui>()->open();
  }

 public:
  MainMenuGui(gfx::gui::NGuiManager* gui, gfx::Engine* engine)
      : NGui(gui, engine) {
    font = gui->getFontCache()->get("engine/gui/eras.ttf", 24);
  }

  virtual void render(gfx::gui::NGuiRenderer* renderer) {
    switch (getGame()->getGameState()->getState()) {
      case GameState::MainMenu:
        renderMainMenu(renderer);
        break;
      case GameState::MenuOnlinePlay:
        break;
      case GameState::Connecting:
        renderConnecting(renderer);
        break;
      case GameState::Todo:
        renderer->text(glm::ivec2(0), font, 0,
                       "TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        break;
      default:
        break;
    }
  }
};

NGUI_INSTANTIATOR(MainMenuGui);

GameState::GameState(Game* game) {
  this->game = game;
  state = cl_nointro.getBool() ? MainMenu : Intro;
  timer = 10.f;
  emitter = game->getSoundManager()->newEmitter();
  entropyLogo = game->getResourceManager()->load<resource::Model>(
      "engine/assets/entropy.obj");
  stateMusic[Todo] = "engine/mus/todo.ogg";

  game->getGfxEngine()->initialized.listen([this, game] {});

  game->getGfxEngine()->renderStepped.listen([this, game] {
    switch (state) {
      default:
      case Connecting:
      case InGame:
        break;
      case MainMenu: {
        renderMainMenu(game->getGfxEngine());
      } break;
      case Todo: {
      } break;
      case Intro: {
        Graph::Node node;
        node.basis = glm::identity<glm::mat3>();
        node.origin = glm::vec3(0);
        gfx::Camera& camera =
            game->getGfxEngine()->getCurrentViewport()->getCamera();
        game->getGfxEngine()->setClearColor(glm::vec3(0));
        float time = game->getGfxEngine()->getTime();
        if (time < 1.0) {
          camera.setTarget(
              glm::vec3(2.f - (time * 2.f), 0.0, 2.f - (time * 2.f)));
        } else if (time > 9.0) {
          camera.setTarget(
              glm::vec3(0.0, glm::mix(0.0, 10.0, 9.f - time), 0.0));
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
