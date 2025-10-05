#include "object.hpp"

#include "logging.hpp"
#include "object_property.hpp"
namespace rdm::reflection {
RDM_REFLECTION_BEGIN_DESCRIBED(Object);
RDM_REFLECTION_PROPERTY_STRING(Object, ClassName, &Object::getClassName, NULL);
RDM_REFLECTION_END_DESCRIBED();

static Reflection* _singleton = NULL;
Reflection* Reflection::singleton() {
  if (!_singleton) _singleton = new Reflection();
  return _singleton;
}

Reflection::Reflection() {
  Class& objectBase = properties["Object"];
  objectBase.name = "Object";
  objectBase.parent = RDM_CLASSDEF_NO_PARENT;
}

PList Reflection::getPListClass(std::string className) {
  if (className == RDM_CLASSDEF_NO_PARENT) return PList();
  if (Reflection::singleton()->properties.find(className) ==
      Reflection::singleton()->properties.end()) {
    return PList();
  } else {
    Class thisClass = Reflection::singleton()->properties[className];
    PList p = getPListClass(thisClass.parent);
    for (auto prop : thisClass.properties) {
      p[prop.first] = prop.second;
    }
    return p;
  }
}

PList Object::getProperties() {
  return Reflection::singleton()->getPListClass(getClassName());
}

void Reflection::execPrecacheFunctions(ResourceManager* mgr) {
  for (auto nclass : properties) {
    if (nclass.second.precache) {
      Log::printf(LOG_DEBUG, "Precaching %s", nclass.second.name.c_str());
      nclass.second.precache(mgr);
    }
  }
}
};  // namespace rdm::reflection
