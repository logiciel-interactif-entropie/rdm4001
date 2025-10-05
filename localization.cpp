#include "localization.hpp"

#include <format>

#include "fun.hpp"
#include "logging.hpp"
#include "subprojects/common/filesystem.hpp"
namespace rdm {
static LocalizationManager* _singleton = NULL;
LocalizationManager* LocalizationManager::singleton() {
  if (!_singleton) _singleton = new LocalizationManager();
  return _singleton;
}

LocalizationManager::LocalizationManager() {
  language = "en";
  addLocalizationBase("engine/lc/");
}

void LocalizationManager::loadStrings() {
  Log::printf(LOG_DEBUG, "Reloading strings");

  localizedStrings.clear();
  for (auto base : languageBases) {
    std::string path = std::format("{}{}.csv", base, language);
    auto _file = common::FileSystem::singleton()->getFileIO(path.c_str(), "r");
    if (!_file) {
      Log::printf(LOG_ERROR, "Unable to load localization %s", path.c_str());
      continue;
    }
    auto file = _file.value();

    std::optional<std::string> lineO = file->getLine();
    while (lineO.has_value()) {
      std::string line = lineO.value();
      std::stringstream stream(line);
      std::string value;
      std::string item;
      int count = 0;
      int token = 0;
      while (std::getline(stream, item, ',')) {
        if (count == 0) {
          token = hash(item.c_str());
        } else if (count == 1) {
          value = item;
        } else {
          value += "\n" + item;
        }

        count++;
      }
      localizedStrings[token] = value;
      lineO = file->getLine();
    }
  }
}

void LocalizationManager::addLocalizationBase(std::string base) {
  languageBases.push_back(base);
  loadStrings();
}

void LocalizationManager::setLanguage(std::string lang) {
  language = lang;
  loadStrings();
}

const char* LocalizationManager::get(int token, const char* subst) const {
  auto it = localizedStrings.find(token);
  if (it == localizedStrings.end()) {
    return subst;
  }
  return it->second.c_str();
}
};  // namespace rdm
