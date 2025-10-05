#pragma once

#include <lua.hpp>
#include <mutex>

#include "object.hpp"
#include "script_context.hpp"

namespace rdm {
class Event;
};

namespace rdm::script {
typedef size_t ClosureId;
class ScriptContext;
class Script : public reflection::Object {
  RDM_OBJECT;
  RDM_OBJECT_DEF(Script, reflection::Object);

  std::mutex threadMutex;
  lua_State* thread;

  ScriptContext* context;
  friend class ScriptContext;

  std::map<Event*, std::vector<ClosureId>> closures;

  std::string source;

  Script(ScriptContext* context);

 public:
  virtual ~Script();

  lua_State* getThread() { return thread; }
  ScriptContext* getContext() { return context; }

  void loadSource(const char* contentPath);
  std::string getSource() { return source; }

  void run();
  void callFunction(std::function<int(lua_State* L)> prep, int function);

  void removeClosuresForEvent(Event* event);

  void requireRead();
};
}  // namespace rdm::script
