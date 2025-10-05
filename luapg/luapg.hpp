#pragma once
#include "game.hpp"
namespace luapg {
class LuaPGGame : public rdm::Game {
  RDM_OBJECT;
  RDM_OBJECT_DEF(LuaPGGame, rdm::Game);

  rdm::Event serverStarting;

 public:
  rdm::Event* getServerStarting() { return &serverStarting; }

  virtual void initialize();

  virtual void initializeServer();
  virtual void initializeClient();
};
};  // namespace luapg
