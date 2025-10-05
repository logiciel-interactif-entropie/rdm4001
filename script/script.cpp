#include "script.hpp"

#include <lauxlib.h>

#include <filesystem.hpp>
#include <lua.hpp>
#include <mutex>
#include <stdexcept>

#include "event.hpp"
#include "logging.hpp"
#include "script_context.hpp"

namespace rdm::script {
RDM_REFLECTION_BEGIN_DESCRIBED(Script);
RDM_REFLECTION_PROPERTY_STRING(Script, Source, &Script::getSource, NULL);
RDM_REFLECTION_END_DESCRIBED();

Script::Script(ScriptContext* context) {
  this->context = context;
  thread = lua_newthread(context->getLuaState());
}

Script::~Script() {
  lua_gc(thread, LUA_GCCOLLECT, 0);
  for (auto p : closures) {
    for (auto id : p.second) {
      p.first->remove(id);
    }
  }
}

void Script::removeClosuresForEvent(Event* event) {
  if (closures.find(event) != closures.end()) closures.erase(event);
}

void Script::loadSource(const char* sourcePath) {
  auto ss = common::FileSystem::singleton()->readFile(sourcePath);
  if (!ss.has_value()) throw std::runtime_error("Could not load script file");
  auto sd = ss.value();
  if (this->source.empty()) this->source = sourcePath;

  switch (luaL_loadbufferx(thread, (const char*)sd.data(), sd.size(),
                           sourcePath, NULL)) {
    case LUA_OK:
      Log::printf(LOG_DEBUG, "Ok loading chunk");
      break;
    case LUA_ERRSYNTAX:
      Log::printf(LOG_ERROR, "%s", lua_tostring(thread, -1));
      break;
    default:
      Log::printf(LOG_ERROR, "Misc. error on loadSource");
      break;
  }
}

void Script::run() {
  std::scoped_lock lock(threadMutex);
  switch (lua_pcall(thread, 0, LUA_MULTRET, 0)) {
    case LUA_OK:
      break;
    case LUA_ERRRUN:
      Log::printf(LOG_ERROR, "%s", lua_tostring(thread, -1));
      break;
    default:
      Log::printf(LOG_ERROR, "Misc. error on run");
      break;
  }
}

void Script::callFunction(std::function<int(lua_State* L)> prep, int function) {
  std::scoped_lock lock(threadMutex);
  int nargs = 0;
  if (prep) nargs = prep(thread);
  lua_rawgeti(thread, LUA_REGISTRYINDEX, function);
  switch (lua_pcall(thread, nargs, LUA_MULTRET, 0)) {
    case LUA_OK:
      break;
    case LUA_ERRRUN:
      Log::printf(LOG_ERROR, "%s", lua_tostring(thread, -1));
      break;
    default:
      Log::printf(LOG_ERROR, "Misc. error on run");
      break;
  }
}

void Script::requireRead() {
  switch (lua_pcall(thread, 0, 1, 0)) {
    case LUA_OK:
      if (lua_isnil(thread, -1)) {
        Log::printf(LOG_WARN, "Require returned nil");
      }
      break;
    case LUA_ERRRUN:
      Log::printf(LOG_ERROR, "%s", lua_tostring(thread, -1));
      break;
    default:
      Log::printf(LOG_ERROR, "Misc. error on run");
      break;
  }
}
}  // namespace rdm::script
