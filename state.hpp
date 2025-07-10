#pragma once
#include <functional>

#include "gfx/engine.hpp"
#include "resource.hpp"
#include "sound.hpp"
namespace rdm {
class Game;

class GameState {
  Game* game;
  float timer;
  resource::Model* entropyLogo;
  rdm::SoundEmitter* emitter;

 public:
  GameState(Game* game);

  enum States {
    Intro,
    MainMenu,
    MenuOnlinePlay,
    InGame,
    Connecting,
    Todo,
  };

  std::map<States, std::string> stateMusic;
  Signal<> switchingState;

  void setState(States s) {
    state = s;
    switchingState.fire();
  }

  States getState() { return state; }

  virtual void renderMainMenu(gfx::Engine* engine) {};

 private:
  States state;
};

template <typename T>
GameState* GameStateConstructor(Game* game) {
  return new T(game);
}
typedef std::function<GameState*(Game* game)> GameStateConstructorFunction;
};  // namespace rdm
