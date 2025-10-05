#pragma once
#include <glm/fwd.hpp>
#include <lua.hpp>

#include "object.hpp"
namespace rdm::script {

class ObjectBridge {
  static int index(lua_State* L);
  static int newindex(lua_State* L);
  static int gc(lua_State* L);

  //  static int _new(lua_State* L);

 public:
  static void add(lua_State* l);

  static void pushDescribed(lua_State* L, reflection::Object* described);
  static reflection::Object* getDescribed(lua_State* L, unsigned int idx);

  template <typename T>
  static T* getDescribed(lua_State* L, unsigned int idx) {
    reflection::Object* obj = getDescribed(L, idx);
    if (obj) {
      T* obj_t = dynamic_cast<T*>(obj);
      if (!obj_t) {
        throw std::runtime_error("Improper type cast");
      }
      return obj_t;
    }
    throw std::runtime_error("Invalid object");
  }
};

class EventBridge {
  static int index(lua_State* L);
  static int newindex(lua_State* L);

 public:
  static void add(lua_State* l);

  static void pushEvent(lua_State* L, Event* event);
  static Event* getEvent(lua_State* L, unsigned int idx);
};

class VectorBridge {
  static int index(lua_State* L);
  static int newindex(lua_State* L);

  static int m_add(lua_State* L);
  static int m_sub(lua_State* L);
  static int m_mul(lua_State* L);
  static int m_div(lua_State* L);
  static int m_unm(lua_State* L);
  static int m_eq(lua_State* L);

  static int new3(lua_State* L);
  static int new2(lua_State* L);
  static int new4(lua_State* L);

 public:
  static void add(lua_State* l);

  // 0 if not a vector
  static int getVecDimensions(lua_State* L, unsigned int idx);

  static void pushVec3(lua_State* L, glm::vec3 v);
  static glm::vec3* getVec3(lua_State* L, unsigned int idx);
  static void pushVec2(lua_State* L, glm::vec2 v);
  static glm::vec2* getVec2(lua_State* L, unsigned int idx);
  static void pushVec4(lua_State* L, glm::vec4 v);
  static glm::vec4* getVec4(lua_State* L, unsigned int idx);
};

void ScriptAPIInit(lua_State* L);

}  // namespace rdm::script
