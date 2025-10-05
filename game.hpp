#pragma once
#include <SDL3/SDL.h>

#include <memory>

#include "console.hpp"
#include "gfx/engine.hpp"
#include "object.hpp"
#include "resource.hpp"
#include "script/script_context.hpp"
#include "security.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "window.hpp"
#include "world.hpp"

namespace rdm {
class Game : public reflection::Object {
  RDM_OBJECT;
  RDM_OBJECT_DEF(Game, reflection::Object);

 protected:
  std::unique_ptr<Console> console;
  std::unique_ptr<World> worldServer;
  std::unique_ptr<World> world;
  std::unique_ptr<gfx::Engine> gfxEngine;
  std::unique_ptr<SoundManager> soundManager;
  std::unique_ptr<GameState> gameState;
  std::unique_ptr<ResourceManager> resourceManager;
  std::unique_ptr<SecurityManager> securityManager;
  std::unique_ptr<AbstractionWindow> window;

 private:
  std::string iconImg;
  bool dirtyIcon;
  bool ignoreNextMouseMoveEvent;
  bool initialized;
  bool silence;
  WorldConstructorSettings worldSettings;
  void updateIcon();

 public:
  Game(bool silence = false);
  virtual ~Game();

  // start the game state mode
  void startGameState(GameStateConstructorFunction f);

  WorldConstructorSettings& getWorldConstructorSettings() {
    return worldSettings;
  }

  // call before accessing world
  void startClient();
  // call before accessing worldServer
  void startServer();

  void lateInitServer();
  void stopServer();

  virtual void initialize() = 0;
  virtual void initializeClient() {};
  virtual void initializeServer() {};

  const char* copyright();

  size_t getVersion();
  virtual size_t getGameVersion() { return 0; };

  void earlyInit();
  void pollEvents();

  void mainLoop();

  void setIcon(std::string icon) {
    iconImg = icon;
    dirtyIcon = true;
  }

  std::string getIcon() { return iconImg; }

  Console* getConsole() { return console.get(); }

  World* getWorld() { return world.get(); }
  World* getServerWorld() { return worldServer.get(); }

  SoundManager* getSoundManager() { return soundManager.get(); }
  gfx::Engine* getGfxEngine() { return gfxEngine.get(); }

  GameState* getGameState() { return gameState.get(); }

  ResourceManager* getResourceManager() { return resourceManager.get(); }
  SecurityManager* getSecurityManager() { return securityManager.get(); }
};
}  // namespace rdm
