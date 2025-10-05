#include "object_property.hpp"

#include <format>

#include "console.hpp"
#include "logging.hpp"
#include "object.hpp"
namespace rdm::reflection {
void RegisterProp(std::string className, std::string name, Property* property) {
  Reflection::Class& thisClass = Reflection::singleton()->properties[className];
  thisClass.properties[name] = property;
}

static ConsoleCommand dump_reflection_info(
    "dump_reflection_info", "dump_reflection_info", "",
    [](Game* game, ConsoleArgReader args) {
      for (auto [className, rClass] : Reflection::singleton()->properties) {
        Log::printf(LOG_INFO, "Class %s derives from %s", className.c_str(),
                    rClass.parent.c_str());
        for (auto property :
             Reflection::singleton()->getPListClass(className)) {
          Log::printf(LOG_INFO, "\t%s %s",
                      property.second->isWriteable() ? "Writable" : "Read-Only",
                      property.first.c_str());
        }
      }

      for (auto [enumName, rEnum] : Reflection::singleton()->enums) {
        Log::printf(LOG_INFO, "Enum %s", rEnum.name.c_str());
        for (auto enumValue : rEnum.enumValues) {
          Log::printf(LOG_INFO, "\t%s = %i", enumValue.first.c_str(),
                      enumValue.second);
        }
      }
    });
}  // namespace rdm::reflection
