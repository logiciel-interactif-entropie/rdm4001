#include "script_api.hpp"

#include <stdexcept>

#include "filesystem.hpp"
#include "logging.hpp"
#include "script.hpp"
namespace rdm::script {

int ObjectBridge::index(lua_State* L) {
  reflection::Object* object = getDescribed(L, 1);
  const char* name = lua_tostring(L, 2);

  reflection::PList plist = object->getProperties();
  auto it = plist.find(name);
  if (it != plist.end()) {
    reflection::Property* p = it->second;
    switch (p->getType()) {
      case reflection::Property::String:
        lua_pushstring(L, p->getString(object).c_str());
        break;
      case reflection::Property::ObjectRef:
        if (reflection::Object* v = p->getObject(object)) {
          ObjectBridge::pushDescribed(L, v);
        } else {
          lua_pushnil(L);
        }
        break;
      case reflection::Property::Function: {
        lua_CFunction m =
            *(p->getFunction().target<reflection::LuaFunctionT*>());
        if (!m) {
          return luaL_error(
              L,
              "p->getFunction().target<reflection::LuaFunctionT*> returned "
              "null");
        }
        lua_pushcfunction(L, m);
      } break;
      case reflection::Property::Bool: {
        lua_pushboolean(L, p->getBool(object));
      } break;
      case reflection::Property::Vec3: {
        VectorBridge::pushVec3(L, p->getVec3(object));
      } break;
      case reflection::Property::Vec2: {
        VectorBridge::pushVec3(L, p->getVec3(object));
      } break;
      case reflection::Property::Vec4: {
        VectorBridge::pushVec4(L, p->getVec4(object));
      } break;
      case reflection::Property::Integer: {
        lua_pushinteger(L, p->getInt(object));
      } break;
      case reflection::Property::Float: {
        lua_pushinteger(L, p->getFloat(object));
      } break;
      case reflection::Property::Signal: {
        EventBridge::pushEvent(L, p->getEvent(object));
      } break;
      default:
        return luaL_error(L, "Invalid access on property %s (DEVELOPER FIXME)",
                          name);
        break;
    }
    return 1;
  }

  /*if (Object* instance = dynamic_cast<Instance*>(object)) {
    if (Instance* find = instance->findFirstChildOfName(name)) {
      ObjectBridge::pushDescribed(L, find);
      return 1;
    }
    }*/

  return luaL_error(L, "Invalid access on property %s", name);
}

int ObjectBridge::newindex(lua_State* L) {
  reflection::Object* object = getDescribed(L, 1);
  const char* name = lua_tostring(L, 2);

  reflection::PList plist = object->getProperties();
  auto it = plist.find(name);
  if (it != plist.end()) {
    reflection::Property* p = it->second;
    if (!p->isWriteable())
      return luaL_error(L, "Could not set unwritable property");

    switch (p->getType()) {
      case reflection::Property::String:
        p->setString(object, lua_tostring(L, 3));
        break;
      case reflection::Property::ObjectRef:
        if (lua_isnil(L, 3)) {
          p->setObject(object, NULL);
          break;
        }
        if (reflection::Object* i = ObjectBridge::getDescribed(L, 3)) {
          p->setObject(object, i);
        } else {
          return luaL_error(L, "Attempt to set to a nil value");
        }
        break;
      case reflection::Property::Vec3:
        p->setVec3(object, *VectorBridge::getVec3(L, 3));
        break;
      case reflection::Property::Vec4:
        p->setVec4(object, *VectorBridge::getVec4(L, 3));
        break;
      case reflection::Property::Vec2:
        p->setVec2(object, *VectorBridge::getVec2(L, 3));
        break;
      case reflection::Property::Bool:
        p->setBool(object, lua_toboolean(L, 3));
        break;
      case reflection::Property::Integer:
        p->setInt(object, lua_tointeger(L, 3));
        break;
      case reflection::Property::Float:
        p->setFloat(object, lua_tonumber(L, 3));
        break;
      default:
        rdm::Log::printf(rdm::LOG_ERROR, "Attempted access on property %s",
                         name);
        return luaL_error(L, "Invalid access on property (DEVELOPER FIXME)");
        break;
    }
    return 0;
  }

  return luaL_error(L, "Invalid access on property %s", name);
}

int ObjectBridge::gc(lua_State* L) {
  reflection::Object** ud =
      (reflection::Object**)luaL_checkudata(L, 1, "Described");
  reflection::Object* d = *ud;
  // d->gcRmReference();
  return 0;
}

void ObjectBridge::add(lua_State* L) {
  static const struct luaL_Reg lib[] = {{NULL, NULL}};

  luaL_newmetatable(L, "Described");

  lua_pushstring(L, "__metatable");
  lua_pushstring(L, "NUNYA");
  lua_settable(L, -3);

  lua_pushstring(L, "type");
  lua_pushstring(L, "Described");
  lua_settable(L, -3);

  lua_pushstring(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);

  lua_pushstring(L, "__newindex");
  lua_pushcfunction(L, newindex);
  lua_settable(L, -3);

  lua_pushstring(L, "__gc");
  lua_pushcfunction(L, gc);
  lua_settable(L, -3);

  lua_pop(L, 1);

  lua_newtable(L);
  luaL_setfuncs(L, lib, 0);
  lua_setglobal(L, "Described");
}

/*int ObjectBridge::_new(lua_State* L) {
Instance* script = ScriptAPI::getScriptObj(L);
const char* type = lua_tostring(L, 1);

if (lua_gettop(L) == 1) {
  Instance* i = InstanceFactory::singleton()->create(type, script->getDM());
  if (!i) luaL_error(L, "Unknown instance of type %s", type);
  pushDescribed(L, i);
} else if (lua_gettop(L) == 2) {
  Instance* p = dynamic_cast<Instance*>(ObjectBridge::getDescribed(L, 2));
  Instance* i = InstanceFactory::singleton()->create(type, script->getDM());
  if (!i) luaL_error(L, "Unknown instance of type %s", type);
  i->setParent(p);
  pushDescribed(L, i);
} else {
  throw std::runtime_error("");
}

return 1;
}*/

reflection::Object* ObjectBridge::getDescribed(lua_State* L, unsigned int idx) {
  void** ud = (void**)luaL_checkudata(L, idx, "Described");
  return (reflection::Object*)(*ud);
}

void ObjectBridge::pushDescribed(lua_State* L, reflection::Object* described) {
  reflection::Object** value =
      (reflection::Object**)lua_newuserdata(L, sizeof(reflection::Object*));
  *value = described;
  // (*value)->gcAddReference();
  luaL_getmetatable(L, "Described");
  lua_setmetatable(L, -2);
}

static int __event_connect(lua_State* L) {
  Event* event = EventBridge::getEvent(L, 1);
  int function = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_getglobal(L, "script");
  Script* script = dynamic_cast<Script*>(ObjectBridge::getDescribed(L, -1));
  lua_pop(L, -1);

  if (!script) throw std::runtime_error("No script");

  event->connect(script, function);

  return 0;
}

int EventBridge::index(lua_State* L) {
  Event* event = getEvent(L, 1);
  const char* name = lua_tostring(L, 2);

  if (std::string(name) == "connect") {
    lua_pushcfunction(L, __event_connect);
  } else {
    throw std::runtime_error("No such field");
  }

  return 1;
}

int EventBridge::newindex(lua_State* L) {
  throw std::runtime_error("You're not allowed to do that");
}

void EventBridge::add(lua_State* L) {
  static const struct luaL_Reg lib[] = {{NULL, NULL}};

  luaL_newmetatable(L, "Event");

  lua_pushstring(L, "__metatable");
  lua_pushstring(L, "Event Bro");
  lua_settable(L, -3);

  lua_pushstring(L, "type");
  lua_pushstring(L, "Described");
  lua_settable(L, -3);

  lua_pushstring(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);

  lua_pushstring(L, "__newindex");
  lua_pushcfunction(L, newindex);
  lua_settable(L, -3);

  lua_pop(L, 1);

  lua_newtable(L);
  luaL_setfuncs(L, lib, 0);
  lua_setglobal(L, "Event");
}

int VectorBridge::index(lua_State* L) {
  int dimensions = getVecDimensions(L, 1);
  std::string name = lua_tostring(L, 2);

  glm::vec4 v;
  switch (dimensions) {
    case 2:
      v = glm::vec4(*getVec2(L, 1), 0, 0);
      break;
    case 3:
      v = glm::vec4(*getVec3(L, 1), 0);
      break;
    case 4:
      v = *getVec4(L, 1);
      break;
  }

  if (name == "x") {
    lua_pushnumber(L, v.x);
    return 1;
  } else if (name == "y") {
    lua_pushnumber(L, v.y);
    return 1;
  } else if (name == "z") {
    if (dimensions < 3) return 0;
    lua_pushnumber(L, v.z);
    return 1;
  } else if (name == "w") {
    if (dimensions < 4) return 0;
    lua_pushnumber(L, v.w);
    return 1;
  } else {
    return 0;
  }
}

int VectorBridge::newindex(lua_State* L) {
  int dimensions = getVecDimensions(L, 1);
  std::string name = lua_tostring(L, 2);

  glm::vec4 v;
  switch (dimensions) {
    case 2:
      v = glm::vec4(*getVec2(L, 1), 0, 0);
      break;
    case 3:
      v = glm::vec4(*getVec3(L, 1), 0);
      break;
    case 4:
      v = *getVec4(L, 1);
      break;
  }

  if (name == "x") {
    v.x = lua_tonumber(L, 3);
  } else if (name == "y") {
    v.y = lua_tonumber(L, 3);
  } else if (name == "z") {
    if (dimensions < 3)
      return luaL_error(L, "Invalid dimensions (has %i, needs %i)", dimensions,
                        3);
    v.z = lua_tonumber(L, 3);
  } else if (name == "w") {
    if (dimensions < 4)
      return luaL_error(L, "Invalid dimensions (has %i, needs %i)", dimensions,
                        4);
    v.w = lua_tonumber(L, 3);
  }

  switch (dimensions) {
    case 4:
      *getVec4(L, 1) = v;
      break;
    case 3:
      *getVec3(L, 1) = glm::vec3(v.x, v.y, v.z);
      break;
    case 2:
      *getVec2(L, 1) = glm::vec2(v.x, v.y);
      break;
  }

  return 0;
}

int VectorBridge::getVecDimensions(lua_State* L, unsigned int idx) {
  if (luaL_testudata(L, idx, "Vector2")) return 2;
  if (luaL_testudata(L, idx, "Vector3")) return 3;
  if (luaL_testudata(L, idx, "Vector4")) return 4;
  return 0;
}

void VectorBridge::pushVec3(lua_State* L, glm::vec3 v) {
  glm::vec3* value = (glm::vec3*)lua_newuserdata(L, sizeof(glm::vec3));
  *value = v;
  luaL_getmetatable(L, "Vector3");
  lua_setmetatable(L, -2);
}

glm::vec3* VectorBridge::getVec3(lua_State* L, unsigned int idx) {
  if (getVecDimensions(L, idx) != 3)
    throw std::runtime_error("Invalid dimensions");
  glm::vec3* ud = (glm::vec3*)luaL_checkudata(L, idx, "Vector3");
  return ud;
}

void VectorBridge::pushVec2(lua_State* L, glm::vec2 v) {
  glm::vec2* value = (glm::vec2*)lua_newuserdata(L, sizeof(glm::vec2));
  *value = v;
  luaL_getmetatable(L, "Vector2");
  lua_setmetatable(L, -2);
}

glm::vec2* VectorBridge::getVec2(lua_State* L, unsigned int idx) {
  if (getVecDimensions(L, idx) != 2)
    throw std::runtime_error("Invalid dimensions");
  glm::vec2* ud = (glm::vec2*)luaL_checkudata(L, idx, "Vector2");
  return ud;
}

void VectorBridge::pushVec4(lua_State* L, glm::vec4 v) {
  glm::vec3* value = (glm::vec3*)lua_newuserdata(L, sizeof(glm::vec3));
  *value = v;
  luaL_getmetatable(L, "Vector4");
  lua_setmetatable(L, -2);
}

glm::vec4* VectorBridge::getVec4(lua_State* L, unsigned int idx) {
  if (getVecDimensions(L, idx) != 4)
    throw std::runtime_error("Invalid dimensions");
  glm::vec4* ud = (glm::vec4*)luaL_checkudata(L, idx, "Vector4");
  return ud;
}

int VectorBridge::m_add(lua_State* L) {
  switch (getVecDimensions(L, 1)) {
    case 2:
      pushVec2(L, *getVec2(L, 1) + *getVec2(L, 2));
      break;
    case 3:
      pushVec3(L, *getVec3(L, 1) + *getVec3(L, 2));
      break;
    case 4:
      pushVec4(L, *getVec4(L, 1) + *getVec4(L, 2));
      break;
  }
  return 1;
}

int VectorBridge::m_sub(lua_State* L) {
  switch (getVecDimensions(L, 1)) {
    case 2:
      pushVec2(L, *getVec2(L, 1) - *getVec2(L, 2));
      break;
    case 3:
      pushVec3(L, *getVec3(L, 1) - *getVec3(L, 2));
      break;
    case 4:
      pushVec4(L, *getVec4(L, 1) - *getVec4(L, 2));
      break;
  }
  return 1;
}

int VectorBridge::m_mul(lua_State* L) {
  bool scalar = false;
  if (getVecDimensions(L, 2) == 0) {
    scalar = true;
  }

  switch (getVecDimensions(L, 1)) {
    case 2:
      if (!scalar)
        pushVec2(L, *getVec2(L, 1) * *getVec2(L, 2));
      else
        pushVec2(L, *getVec2(L, 1) * (float)lua_tonumber(L, 2));
      break;
    case 3:
      if (!scalar)
        pushVec3(L, *getVec3(L, 1) * *getVec3(L, 2));
      else
        pushVec3(L, *getVec3(L, 1) * (float)lua_tonumber(L, 2));
      break;
    case 4:
      if (!scalar)
        pushVec4(L, *getVec4(L, 1) * *getVec4(L, 2));
      else
        pushVec4(L, *getVec4(L, 1) * (float)lua_tonumber(L, 2));
      break;
  }
  return 1;
}

int VectorBridge::m_div(lua_State* L) {
  bool scalar = false;
  if (getVecDimensions(L, 2) == 0) {
    scalar = true;
  }

  switch (getVecDimensions(L, 1)) {
    case 2:
      if (!scalar)
        pushVec2(L, *getVec2(L, 1) / *getVec2(L, 2));
      else
        pushVec2(L, *getVec2(L, 1) / (float)lua_tonumber(L, 2));
      break;
    case 3:
      if (!scalar)
        pushVec3(L, *getVec3(L, 1) / *getVec3(L, 2));
      else
        pushVec3(L, *getVec3(L, 1) / (float)lua_tonumber(L, 2));
      break;
    case 4:
      if (!scalar)
        pushVec4(L, *getVec4(L, 1) / *getVec4(L, 2));
      else
        pushVec4(L, *getVec4(L, 1) / (float)lua_tonumber(L, 2));
      break;
  }
  return 1;
}

int VectorBridge::m_unm(lua_State* L) {
  switch (getVecDimensions(L, 1)) {
    case 2:
      pushVec2(L, -(*getVec2(L, 1)));
      break;
    case 3:
      pushVec3(L, -(*getVec3(L, 1)));
      break;
    case 4:
      pushVec4(L, -(*getVec4(L, 1)));
      break;
  }
  return 1;
}

int VectorBridge::m_eq(lua_State* L) {
  switch (getVecDimensions(L, 1)) {
    case 2:
      lua_pushboolean(L, *getVec2(L, 1) == *getVec2(L, 2));
      break;
    case 3:
      lua_pushboolean(L, *getVec3(L, 1) == *getVec3(L, 2));
      break;
    case 4:
      lua_pushboolean(L, *getVec4(L, 1) == *getVec4(L, 2));
      break;
  }
  return 1;
}

void VectorBridge::add(lua_State* L) {
  static const struct luaL_Reg lib3[] = {{"new", &new3}, {NULL, NULL}};
  static const struct luaL_Reg lib2[] = {{"new", &new2}, {NULL, NULL}};
  static const struct luaL_Reg lib4[] = {{"new", &new4}, {NULL, NULL}};

  luaL_newmetatable(L, "Vector4");

  lua_pushstring(L, "__metatable");
  lua_pushstring(L, "Vector4");
  lua_settable(L, -3);

  lua_pushstring(L, "type");
  lua_pushstring(L, "Described");
  lua_settable(L, -3);

  lua_pushstring(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);

  lua_pushstring(L, "__newindex");
  lua_pushcfunction(L, newindex);
  lua_settable(L, -3);

  lua_pushstring(L, "__add");
  lua_pushcfunction(L, m_add);
  lua_settable(L, -3);

  lua_pushstring(L, "__sub");
  lua_pushcfunction(L, m_sub);
  lua_settable(L, -3);

  lua_pushstring(L, "__mul");
  lua_pushcfunction(L, m_mul);
  lua_settable(L, -3);

  lua_pushstring(L, "__div");
  lua_pushcfunction(L, m_div);
  lua_settable(L, -3);

  lua_pushstring(L, "__unm");
  lua_pushcfunction(L, m_unm);
  lua_settable(L, -3);

  lua_pushstring(L, "__eq");
  lua_pushcfunction(L, m_eq);
  lua_settable(L, -3);

  lua_pop(L, 1);

  luaL_newmetatable(L, "Vector3");

  lua_pushstring(L, "__metatable");
  lua_pushstring(L, "VEC");
  lua_settable(L, -3);

  lua_pushstring(L, "type");
  lua_pushstring(L, "Vector3");
  lua_settable(L, -3);

  lua_pushstring(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);

  lua_pushstring(L, "__newindex");
  lua_pushcfunction(L, newindex);
  lua_settable(L, -3);

  lua_pushstring(L, "__add");
  lua_pushcfunction(L, m_add);
  lua_settable(L, -3);

  lua_pushstring(L, "__sub");
  lua_pushcfunction(L, m_sub);
  lua_settable(L, -3);

  lua_pushstring(L, "__mul");
  lua_pushcfunction(L, m_mul);
  lua_settable(L, -3);

  lua_pushstring(L, "__div");
  lua_pushcfunction(L, m_div);
  lua_settable(L, -3);

  lua_pushstring(L, "__unm");
  lua_pushcfunction(L, m_unm);
  lua_settable(L, -3);

  lua_pushstring(L, "__eq");
  lua_pushcfunction(L, m_eq);
  lua_settable(L, -3);

  lua_pop(L, 1);

  luaL_newmetatable(L, "Vector2");

  lua_pushstring(L, "__metatable");
  lua_pushstring(L, "VEC");
  lua_settable(L, -3);

  lua_pushstring(L, "type");
  lua_pushstring(L, "Vector2");
  lua_settable(L, -3);

  lua_pushstring(L, "__index");
  lua_pushcfunction(L, index);
  lua_settable(L, -3);

  lua_pushstring(L, "__newindex");
  lua_pushcfunction(L, newindex);
  lua_settable(L, -3);

  lua_pushstring(L, "__add");
  lua_pushcfunction(L, m_add);
  lua_settable(L, -3);

  lua_pushstring(L, "__sub");
  lua_pushcfunction(L, m_sub);
  lua_settable(L, -3);

  lua_pushstring(L, "__mul");
  lua_pushcfunction(L, m_mul);
  lua_settable(L, -3);

  lua_pushstring(L, "__div");
  lua_pushcfunction(L, m_div);
  lua_settable(L, -3);

  lua_pushstring(L, "__unm");
  lua_pushcfunction(L, m_unm);
  lua_settable(L, -3);

  lua_pushstring(L, "__eq");
  lua_pushcfunction(L, m_eq);
  lua_settable(L, -3);

  lua_pop(L, 1);

  lua_newtable(L);
  luaL_setfuncs(L, lib2, 0);
  lua_setglobal(L, "Vector2");

  lua_newtable(L);
  luaL_setfuncs(L, lib3, 0);
  lua_setglobal(L, "Vector3");

  lua_newtable(L);
  luaL_setfuncs(L, lib4, 0);
  lua_setglobal(L, "Vector4");
}

int VectorBridge::new2(lua_State* L) {
  glm::vec2 v;
  if (lua_gettop(L) == 0) {
    v = glm::vec2(0.0);
  } else if (lua_gettop(L) == 2) {
    v = glm::vec2(lua_tonumber(L, 1), lua_tonumber(L, 2));
  }
  VectorBridge::pushVec2(L, v);
  return 1;
}

int VectorBridge::new3(lua_State* L) {
  glm::vec3 v;
  if (lua_gettop(L) == 0) {
    v = glm::vec3(0.0);
  } else if (lua_gettop(L) == 3) {
    v = glm::vec3(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3));
  }
  VectorBridge::pushVec3(L, v);
  return 1;
}

int VectorBridge::new4(lua_State* L) {
  glm::vec4 v;
  if (lua_gettop(L) == 0) {
    v = glm::vec4(0.0);
  } else if (lua_gettop(L) == 4) {
    v = glm::vec4(lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3),
                  lua_tonumber(L, 4));
  }
  VectorBridge::pushVec4(L, v);
  return 1;
}

Event* EventBridge::getEvent(lua_State* L, unsigned int idx) {
  void** ud = (void**)luaL_checkudata(L, idx, "Event");
  return (Event*)(*ud);
}

void EventBridge::pushEvent(lua_State* L, Event* described) {
  Event** value = (Event**)lua_newuserdata(L, sizeof(Event*));
  *value = described;
  // (*value)->gcAddReference();
  luaL_getmetatable(L, "Event");
  lua_setmetatable(L, -2);
}

static int __print(lua_State* L) {
  std::string s;
  for (int i = 0; i < lua_gettop(L); i++) {
    const char* str = lua_tolstring(L, i + 1, NULL);
    if (str == NULL) {
      if (lua_isnil(L, i + 1)) {
        str = "nil";
      } else {
        str = lua_typename(L, i + 1);
      }
    }
    s += str + std::string(" ");
    lua_pop(L, 1);
  }
  rdm::Log::printf(rdm::LOG_INFO, "%s", s.c_str());
  return 0;
}

static int __error(lua_State* L) {
  std::string s;
  for (int i = 0; i < lua_gettop(L); i++) {
    const char* str = lua_tolstring(L, i + 1, NULL);
    if (str == NULL) str = "nil";
    s += str + std::string(" ");
    lua_pop(L, 1);
  }
  rdm::Log::printf(rdm::LOG_ERROR, "%s", s.c_str());
  return 0;
}

static int __warn(lua_State* L) {
  std::string s;
  for (int i = 0; i < lua_gettop(L); i++) {
    const char* str = lua_tolstring(L, i + 1, NULL);
    if (str == NULL) str = "nil";
    s += str + std::string(" ");
    lua_pop(L, 1);
  }
  rdm::Log::printf(rdm::LOG_WARN, "%s", s.c_str());
  return 0;
}

static int __require(lua_State* L) {
  std::string path = lua_tostring(L, 1);

  lua_getglobal(L, "script");
  Script* script = dynamic_cast<Script*>(ObjectBridge::getDescribed(L, -1));
  lua_pop(L, -1);

  auto ss = common::FileSystem::singleton()->readFile(path.c_str());
  if (ss.has_value()) {
    script->loadSource(path.c_str());
    script->requireRead();
  } else {
    return luaL_error(L, "Could not load script at %s", path.c_str());
  }

  return 1;
}

void ScriptAPIInit(lua_State* L) {
  ObjectBridge::add(L);
  EventBridge::add(L);
  VectorBridge::add(L);

  for (auto r : reflection::Reflection::singleton()->enums) {
    lua_newtable(L);
    for (auto v : r.second.enumValues) {
      lua_pushstring(L, v.first.c_str());
      lua_pushinteger(L, v.second);
      lua_settable(L, -3);
    }
    lua_setglobal(L, r.second.name.c_str());
  }

  lua_pushcfunction(L, &__print);
  lua_setglobal(L, "print");
  lua_pushcfunction(L, &__warn);
  lua_setglobal(L, "warn");
  lua_pushcfunction(L, &__error);
  lua_setglobal(L, "error");
  lua_pushcfunction(L, &__require);
  lua_setglobal(L, "require");
}
}  // namespace rdm::script
