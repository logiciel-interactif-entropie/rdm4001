#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "event.hpp"
#include "fun.hpp"
#include "graph.hpp"
#include "network/network.hpp"
#include "object.hpp"
#include "physics.hpp"
#include "scheduler.hpp"
#include "script/script_context.hpp"

namespace rdm {
class Game;

struct WorldConstructorSettings {
  bool network;
  bool physics;
  std::string name;
  Game* game;

  WorldConstructorSettings() {
    network = false;
    physics = false;
    name = Fun::getModuleName();
    game = NULL;
  }
};

class World : public reflection::Object {
  RDM_OBJECT;
  RDM_OBJECT_DEF(World, reflection::Object);

  friend class WorldJob;
  friend class WorldTitleJob;

  std::unique_ptr<Graph> graph;
  std::unique_ptr<PhysicsWorld> physics;
  std::unique_ptr<network::NetworkManager> networkManager;
  std::unique_ptr<script::ScriptContext> scriptContext;
  std::unique_ptr<Scheduler> scheduler;
  void* user;
  Game* game;
  std::string title;
  std::string name;
  bool running;
  double time;
  SchedulerJob* worldJob;

  Event luaStepped;

  void tick();

 public:
  World(WorldConstructorSettings settings = WorldConstructorSettings());
  ~World();

  Signal<> stepping;
  Signal<> stepped;
  Signal<std::string> changingTitle;

  std::mutex worldLock;  // lock when writing to world state

  void setTitle(std::string title);
  std::string getTitle() { return title; }

  Event* getLuaStepped() { return &luaStepped; }
  Game* getGame() { return game; }
  Scheduler* getScheduler() { return scheduler.get(); }
  PhysicsWorld* getPhysicsWorld() { return physics.get(); }
  network::NetworkManager* getNetworkManager() { return networkManager.get(); }
  double getTime() { return time; };

  SchedulerJob* getWorldJob() { return worldJob; }

  void setUser(void* p) { user = p; }
  void* getUser() { return user; }

  script::ScriptContext* getScriptContext() { return scriptContext.get(); }
  const char* getName() { return name.c_str(); }

  void setRunning(bool running) { this->running = running; }
  bool getRunning() { return running; };
};
}  // namespace rdm
