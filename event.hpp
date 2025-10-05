#pragma once
#include <stddef.h>

#include <functional>
#include <lua.hpp>
#include <map>

namespace rdm {
namespace script {
class Script;
};

typedef size_t ClosureId;
class Event {
  struct LuaListener {
    script::Script* script;
    int function;
  };

  std::map<ClosureId, LuaListener> listeners;

 public:
  Event();
  ~Event();

  void fire(std::function<int(lua_State* L)> set);
  inline void fire() { fire({}); }
  ClosureId connect(script::Script* script, int function);
  void remove(ClosureId id);
};
}  // namespace rdm
