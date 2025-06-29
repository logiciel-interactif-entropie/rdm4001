#include "apis.hpp"

#include "console.hpp"
namespace rdm::gfx {
static ApiFactory* _singleton = NULL;
ApiFactory* ApiFactory::singleton() {
  if (!_singleton) _singleton = new ApiFactory();
  return _singleton;
}

void ApiFactory::printSupportedApis() {
  for (auto& [name, _] : regs) {
    Log::printf(LOG_INFO, "%s", name.c_str());
  }
}

static ConsoleCommand r_supported_apis(
    "r_supported_apis", "r_supported_apis", "",
    [](Game* game, ConsoleArgReader reader) {
      ApiFactory::singleton()->printSupportedApis();
    });
}  // namespace rdm::gfx
