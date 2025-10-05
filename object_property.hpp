#pragma once
#include <event.hpp>
#include <functional>
#include <glm/glm.hpp>
#include <lua.hpp>
#include <stdexcept>
#include <string>

// taken from freeblock

namespace rdm::reflection {
class Object;

typedef int LuaFunctionT(lua_State*);
typedef std::function<int(lua_State*)> LuaFunction;

class Property {
 protected:
  std::string name;

 public:
  enum Type {
    String,
    Integer,
    Bool,
    Float,
    Vec3,
    Mat3,
    Vec2,
    Vec4,
    ObjectRef,
    Function,
    Signal
  };

  virtual bool isWriteable() { return false; }

  const char* getName() const { return name.c_str(); };
  virtual Type getType() = 0;

  virtual std::string getString(Object* described) {
    throw std::runtime_error("No string");
  }
  virtual void setString(Object* described, std::string str) {
    throw std::runtime_error("No string");
  }

  virtual int getInt(Object* described) { throw std::runtime_error("No int"); }
  virtual void setInt(Object* described, int value) {
    throw std::runtime_error("No int");
  }

  virtual bool getBool(Object* described) {
    throw std::runtime_error("No bool");
  }
  virtual void setBool(Object* described, bool value) {
    throw std::runtime_error("No bool");
  }

  virtual float getFloat(Object* described) {
    throw std::runtime_error("No float");
  }
  virtual void setFloat(Object* described, float value) {
    throw std::runtime_error("No float");
  }

  virtual glm::mat3 getMat3(Object* described) {
    throw std::runtime_error("No mat3");
  }

  virtual void setMat3(Object* described, glm::mat3 str) {
    throw std::runtime_error("No mat3");
  }

  virtual glm::vec3 getVec3(Object* described) {
    throw std::runtime_error("No vec3");
  }
  virtual void setVec3(Object* described, glm::vec3 value) {
    throw std::runtime_error("No vec3");
  }

  virtual glm::vec4 getVec4(Object* described) {
    throw std::runtime_error("No vec4");
  }
  virtual void setVec4(Object* described, glm::vec4 value) {
    throw std::runtime_error("No vec4");
  }

  virtual glm::vec2 getVec2(Object* described) {
    throw std::runtime_error("No vec2");
  }
  virtual void setVec2(Object* described, glm::vec2 value) {
    throw std::runtime_error("No vec2");
  }

  virtual Object* getObject(Object* described) {
    throw std::runtime_error("No instance");
  }
  virtual void setObject(Object* described, Object* instance) {
    throw std::runtime_error("No instance");
  }

  virtual Event* getEvent(Object* described) {
    throw std::runtime_error("No event");
  }

  virtual LuaFunction getFunction() { throw std::runtime_error("No function"); }
};

void RegisterProp(std::string className, std::string name, Property* property);

template <typename T>
class PropertyString : public Property {
  std::function<void(T*, std::string)> setter;
  std::function<std::string(T*)> getter;

 public:
  typedef std::function<void(T*, std::string)> Setter;
  typedef std::function<std::string(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyString(std::string className, std::string name, Setter set,
                 Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return String; }

  virtual std::string getString(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setString(Object* described, std::string str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyInt : public Property {
  typedef int DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyInt(std::string className, std::string name, Setter set, Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Integer; }

  virtual DataType getInt(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setInt(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyBool : public Property {
  typedef bool DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyBool(std::string className, std::string name, Setter set,
               Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Bool; }

  virtual DataType getBool(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setBool(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyFloat : public Property {
  typedef float DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyFloat(std::string className, std::string name, Setter set,
                Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Float; }

  virtual DataType getFloat(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setFloat(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyMat3 : public Property {
  typedef glm::mat3 DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyMat3(std::string className, std::string name, Setter set,
               Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Mat3; }

  virtual DataType getMat3(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setMat3(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyVec3 : public Property {
  typedef glm::vec3 DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyVec3(std::string className, std::string name, Setter set,
               Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Vec3; }

  virtual DataType getVec3(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setVec3(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyVec4 : public Property {
  typedef glm::vec4 DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyVec4(std::string className, std::string name, Setter set,
               Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Vec4; }

  virtual DataType getVec4(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setVec3(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyVec2 : public Property {
  typedef glm::vec2 DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyVec2(std::string className, std::string name, Setter set,
               Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Vec2; }

  virtual DataType getVec2(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setVec2(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyObject : public Property {
  typedef Object* DataType;

  std::function<void(T*, DataType)> setter;
  std::function<DataType(T*)> getter;

 public:
  typedef std::function<void(T*, DataType)> Setter;
  typedef std::function<DataType(T*)> Getter;

  virtual bool isWriteable() { return (setter != nullptr); }

  PropertyObject(std::string className, std::string name, Setter set,
                 Getter get) {
    this->name = name;
    setter = set;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return ObjectRef; }

  virtual DataType getObject(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }

  virtual void setObject(Object* described, DataType str) {
    setter(dynamic_cast<T*>(described), str);
  }
};

template <typename T>
class PropertyFunction : public Property {
  typedef LuaFunction DataType;
  LuaFunction func;

 public:
  PropertyFunction(std::string className, std::string name, LuaFunction func) {
    this->name = name;
    this->func = func;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Function; }

  DataType getFunction() { return func; }
};

template <typename T>
class PropertyEvent : public Property {
  typedef Event* DataType;

  std::function<DataType(T*)> getter;

 public:
  typedef std::function<DataType(T*)> Getter;

  PropertyEvent(std::string className, std::string name, Getter get) {
    this->name = name;
    getter = get;

    RegisterProp(className, name, this);
  }

  virtual Type getType() { return Signal; }

  virtual Event* getEvent(Object* described) {
    return getter(dynamic_cast<T*>(described));
  }
};

#define RDM_REFLECTION_PROPERTY_STRING(T, N, Gt, St) \
  static rdm::reflection::PropertyString<T> __##N(#T, #N, St, Gt);

#define RDM_REFLECTION_PROPERTY_INT(T, N, Gt, St) \
  static rdm::reflection::PropertyInt<T> __##N(#T, #N, St, Gt);

#define RDM_REFLECTION_PROPERTY_BOOL(T, N, Gt, St) \
  static rdm::reflection::PropertyBool<T> __##N(#T, #N, St, Gt);

#define RDM_REFLECTION_PROPERTY_FLOAT(T, N, Gt, St) \
  static rdm::reflection::PropertyFloat<T> __##N(#T, #N, St, Gt);

#define RDM_REFLECTION_PROPERTY_VEC3(T, N, Gt, St) \
  static rdm::reflection::PropertyVec3<T> __##N(#T, #N, St, Gt);

#define RDM_REFLECTION_PROPERTY_MAT3(T, N, Gt, St) \
  static rdm::reflection::PropertyMat3<T> __##N(#T, #N, St, Gt);

#define RDM_REFLECTION_PROPERTY_OBJECT(T, N, Gt, St) \
  static rdm::reflection::PropertyObject<T> __##N(#T, #N, St, Gt);

#define RDM_REFLECTION_PROPERTY_FUNCTION(T, N, Func) \
  static rdm::reflection::PropertyFunction<T> __##N(#T, #N, Func);

#define RDM_REFLECTION_PROPERTY_EVENT(T, N, Gt) \
  static rdm::reflection::PropertyEvent<T> __##N(#T, #N, Gt);
}  // namespace rdm::reflection
