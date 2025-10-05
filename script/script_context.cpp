#include "script_context.hpp"

#include <lua.h>
#include <stdlib.h>

#include <filesystem.hpp>

#include "game.hpp"
#include "script.hpp"
#include "script_api.hpp"
namespace rdm::script {
static void* __l_alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else
    return realloc(ptr, nsize);
}

ScriptContext::ScriptContext(World* world) {
  state = lua_newstate(__l_alloc, this);
  ScriptAPIInit(state);

  ObjectBridge::pushDescribed(state, world->getGame());
  lua_setglobal(state, "game");
  ObjectBridge::pushDescribed(state, world);
  lua_setglobal(state, "world");
}

Script* ScriptContext::newScript(const char* contentPath) {
  auto ss = common::FileSystem::singleton()->readFile(contentPath);
  if (ss.has_value()) {
    auto sd = ss.value();
    Script* script = new Script(this);
    lua_State* l = script->getThread();
    ObjectBridge::pushDescribed(l, script);
    lua_setglobal(l, "script");

    script->loadSource(contentPath);
    return script;
  } else {
    return NULL;
  }
}
}  // namespace rdm::script
