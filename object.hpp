#pragma once
#include <map>
#include <set>
#include <string>

#include "object_property.hpp"
#define RDM_CLASSDEF_NO_PARENT "<<<NO PARENT>>>"

#define RDM_OBJECT \
 public:           \
 private:

#define RDM_OBJECT_DEF(N, P)                                  \
 public:                                                      \
  typedef P Super;                                            \
  typedef N Self;                                             \
  virtual std::string getParentClassName() const {            \
    return P::getClassNameStatic();                           \
  }                                                           \
  virtual std::string getClassName() const { return #N; }     \
  static std::string getClassNameStatic() { return #N; }      \
  static std::string getParentClassNameStatic() {             \
    return P::getClassNameStatic();                           \
  }                                                           \
  template <typename T>                                       \
  static inline bool isA(rdm::reflection::Object* instance) { \
    if (dynamic_cast<T*>(instance)) {                         \
      return true;                                            \
    } else {                                                  \
      return rdm::reflection::Object::isA<Super>(instance);   \
    }                                                         \
  }                                                           \
  virtual inline bool isA(const char* type) const {           \
    if (getClassName() == type) {                             \
      return true;                                            \
    } else {                                                  \
      return Super::isA(type);                                \
    }                                                         \
  }                                                           \
                                                              \
 private:

namespace rdm {
class ResourceManager;
};

namespace rdm::reflection {
typedef std::map<std::string, Property*> PList;

class Object {
 public:
  typedef Object Super;

  static std::string getParentClassNameStatic() {
    return RDM_CLASSDEF_NO_PARENT;
  }
  static std::string getClassNameStatic() { return "Object"; }
  virtual std::string getParentClassName() const { return ""; }
  virtual std::string getClassName() const { return "Object"; }

  virtual bool isA(const char* type) const {
    return (std::string("Object") == type);
  }
  template <typename T>
  static bool isA(Object* instance) {
    if (dynamic_cast<T*>(instance)) {
      return true;
    } else {
      return false;
    }
  }

  PList getProperties();
};

class Constructable : public Object {
  RDM_OBJECT;
  RDM_OBJECT_DEF(Constructable, Object);

  int references;

 public:
  Constructable() { references = 0; }

  void addReference() { references++; }
  void rmReference() { references--; }
  int getReferences() { return references; }
};

class Reflection {
 public:
  Reflection();

  struct Class {
    std::string parent;
    std::string name;
    PList properties;

    typedef std::function<void(ResourceManager*)> PrecacheFunction;

    PrecacheFunction precache;
  };

  struct Enum {
    std::string name;
    std::map<std::string, int> enumValues;
  };

  static Reflection* singleton();

  std::map<std::string, Class> properties;
  std::map<std::string, Enum> enums;

  PList getPListClass(std::string className);

  template <typename T>
  PList get() {
    return getPListClass(T::getClassNameStatic());
  }

  void execPrecacheFunctions(ResourceManager* manager);
};

class ReflectionClassDef {
 public:
  ReflectionClassDef(std::string className, std::string classParent) {
    Reflection::Class& rClass = Reflection::singleton()->properties[className];
    rClass.name = className;
    rClass.parent = classParent;
  }
};

class ReflectionClassPrecacheDef {
 public:
  ReflectionClassPrecacheDef(std::string className,
                             Reflection::Class::PrecacheFunction func) {
    Reflection::Class& rClass = Reflection::singleton()->properties[className];
    rClass.precache = func;
  }
};

class ReflectionEnumDef {
 public:
  ReflectionEnumDef(std::string name, std::map<std::string, int> enumValues) {
    Reflection::Enum& rEnum = Reflection::singleton()->enums[name];
    rEnum.enumValues = enumValues;
    rEnum.name = name;
  }
};

#define RDM_REFLECTION_BEGIN_DESCRIBED(D)               \
  static rdm::reflection::ReflectionClassDef __DEF_##D( \
      #D, D::getParentClassNameStatic());
// this is no longer necessary
#define RDM_REFLECTION_END_DESCRIBED()

#define RDM_REFLECTION_PRECACHE_FUNC(D, F) \
  static rdm::reflection::ReflectionClassPrecacheDef __PCF_##D(#D, F)

#define RDM_REFLECTION_BEGIN_ENUM(N)                        \
  static rdm::reflection::ReflectionEnumDef __DEF_ENUM_##N( \
			   #N, std::map<std::string, int>{
#define RDM_REFLECTION_ENUM(K, V) {#K, (int)V},
#define RDM_REFLECTION_END_ENUM() \
  });
};  // namespace rdm::reflection
