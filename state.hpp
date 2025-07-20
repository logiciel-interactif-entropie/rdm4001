#pragma once
#include <functional>

#include "gfx/engine.hpp"
#include "resource.hpp"
#include "sound.hpp"
namespace rdm {
class Game;

class GameState {
  friend class WaitingWindow;

  Game* game;
  float timer;
  resource::Model* entropyLogo;
  rdm::SoundEmitter* emitter;
  std::string waitingMessage;

 public:
  GameState(Game* game);

  Game* getGame() { return game; }

  enum States {
    Intro,
    FiguringOutWhatToDo,
    WaitForSomething,
    PreGameSetup,
    MainMenu,
    MenuOnlinePlay,
    InGame,
    Connecting,
    Todo,
    Quit,
  };

  std::map<States, std::string> stateMusic;
  Signal<> switchingState;

  void setState(States s) {
    state = s;
    switchingState.fire();
  }

  States getState() { return state; }

  virtual void renderMainMenu(gfx::Engine* engine) {};
  virtual void renderWaiting(gfx::Engine* engine) {};
  // called after intro anim is done
  virtual void figureOutWhatToDo() { setState(MainMenu); };
  virtual void tickWaiting() {};

 private:
  States state;
};

template <typename T>
GameState* GameStateConstructor(Game* game) {
  return new T(game);
}
typedef std::function<GameState*(Game* game)> GameStateConstructorFunction;
};  // namespace rdm
