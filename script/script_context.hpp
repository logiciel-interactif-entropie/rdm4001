#pragma once

#include <lua.hpp>

#include "script.hpp"

namespace rdm {
class World;
}

namespace rdm::script {
class Script;
class ScriptContext {
  lua_State* state;
  World* world;

 public:
  ScriptContext(World* world);

  Script* newScript(const char* contentPath);
  lua_State* getLuaState() { return state; }

  void tick();
};
}  // namespace rdm::script
