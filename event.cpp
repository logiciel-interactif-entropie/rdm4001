#include "event.hpp"

#include "script/script.hpp"
#include "signal.hpp"
namespace rdm {
static ClosureId __last_id = 0;

Event::Event() {}

Event::~Event() {
  for (auto [id, l] : listeners) {
    l.script->removeClosuresForEvent(this);
  }
}

void Event::fire(std::function<int(lua_State* L)> set) {
  for (auto [id, l] : listeners) {
    l.script->callFunction(set, l.function);
  }
}

ClosureId Event::connect(script::Script* script, int function) {
  LuaListener l;
  l.script = script;
  l.function = function;
  ClosureId id = __last_id++;
  listeners[id] = l;
  return id;
}

void Event::remove(ClosureId id) { listeners.erase(listeners.find(id)); }
}  // namespace rdm
