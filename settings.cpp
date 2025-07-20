#include "settings.hpp"

#include <stdio.h>

#include <stdexcept>

#include "json.hpp"
using json = nlohmann::json;

#include <format>
#include <iostream>
#include <string>

#include "fun.hpp"
#include "json.hpp"
#include "logging.hpp"

namespace rdm {
struct SettingsPrivate {
 public:
  json oldSettings;
  json oldSettingsPrivate;
};

Settings::Settings() {
#ifdef NDEBUG
  settingsPath = Fun::getLocalDataDirectory() + "settings.json";
#else
  settingsPath = Fun::getLocalDataDirectory() + "settings_DEBUG.json";
#endif
  settingsPrivatePath = Fun::getLocalDataDirectory() + ".private.json";
  p = new SettingsPrivate;
}

CVar::CVar(const char* name, const char* defaultVar, unsigned long flags) {
  this->name = name;
  this->flags = flags;
  dirty = true;
  value = defaultVar;
  this->defaultVar = defaultVar;
  Settings::singleton()->addCvar(name, this);
}

void CVar::setValue(std::string s) {
  if (s != this->value) {
    this->value = s;
    if (flags & CVARF_NOTIFY) Settings::singleton()->cvarChanging.fire(name);
    changing.fire();
  }
}

int CVar::getInt() { return std::stoi(value); }

void CVar::setInt(int i) { setValue(std::to_string(i)); }

float CVar::getFloat() { return std::stof(value); }

void CVar::setFloat(float f) { setValue(std::to_string(f)); }

// taken from
// https://github.com/floralrainfall/matrix/blob/trunk/matrix/src/mcvar.cpp

bool CVar::getBool() {
  if (value == "false") return false;
  if (value == "0") return false;
  return true;
}

void CVar::setBool(bool b) { setValue(b ? "1" : "0"); }

glm::vec2 CVar::getVec2() {
  glm::vec4 v = getVec4(2);
  return glm::vec2(v.x, v.y);
}

void CVar::setVec2(glm::vec2 v) { setValue(std::format("{} {}", v.x, v.y)); }

glm::vec3 CVar::getVec3() {
  glm::vec4 v = getVec4(3);
  return glm::vec3(v.x, v.y, v.z);
}

void CVar::setVec3(glm::vec3 v) {
  setValue(std::format("{} {} {}", v.x, v.y, v.z));
}

glm::vec4 CVar::getVec4(int ms) { return Math::stringToVec4(value); }

void CVar::setVec4(glm::vec4 v) {
  setValue(std::format("{} {} {} {}", v.x, v.y, v.z, v.w));
}

static Settings* _singleton = 0;
Settings* Settings::singleton() {
  if (!_singleton) _singleton = new Settings();
  return _singleton;
}

void Settings::listCvars() {
  for (auto& var : cvars) {
    if (var.second->getFlags() & CVARF_HIDDEN) continue;

    std::string flags;
    if (var.second->getFlags() & CVARF_GLOBAL) flags += "global ";
    if (var.second->getFlags() & CVARF_NOTIFY) flags += "notify ";
    if (var.second->getFlags() & CVARF_REPLICATE) flags += "replicate ";
    if (var.second->getFlags() & CVARF_SAVE) flags += "save ";

    Log::printf(LOG_INFO, "%s = \"%s\" %s", var.first.c_str(),
                var.second->getValue().c_str(), flags.c_str());
  }
}

void Settings::parseCommandLine(char* argv[], int argc) {
  /*  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()("help,h", "produce help message")(
      "loadSettings,L", po::value<std::string>(),
      "load settings from custom location")(
      "game,g", po::value<std::string>(),
      "loaded game library path (only works on supported programs like the "
      "launcher)")(
      "hintDs,D",
      "Hint to use dedicated server mode (only works on supported programs)")(
      "hintConnectIp,C", po::value<std::string>(),
      "Hint to connect to server (only works on supported programs)")(
      "hintConnectPort,P", po::value<int>(),
      "Hint to connect to port (only works on supported programs)");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    exit(EXIT_FAILURE);
  }

  if (vm.count("loadSettings")) {
    settingsPath = vm["loadSettings"].as<std::string>();
    load();
  }

  if (vm.count("game")) {
    gamePath = vm["game"].as<std::string>();
  }

  if (vm.count("hintConnectIp")) {
    hintConnect = vm["hintConnectIp"].as<std::string>();
  } else {
    hintConnect = "";
  }

  if (vm.count("hintConnectPort")) {
    hintConnectPort = vm["hintConnectPort"].as<int>();
  } else {
    hintConnectPort = 7938;
  }

  hintDs = vm.count("hintDs");
  */

  load();

  for (int i = 0; i < argc; i++) {
    if (argv[i][0] == '+') {
      std::string nam = std::string(argv[i]).substr(1);
      std::string value = argv[++i];
      if (CVar* cvar = getCvar(nam.c_str(), true)) {
        cvar->setValue(value);
      } else {
        Log::printf(LOG_ERROR, "Unknown cvar %s", nam.c_str());
        throw std::runtime_error("Unknown cvar");
      }
    } else if (argv[i][0] == '-') {
      bool longArg = argv[i][1] == '-';
      std::string arg = std::string(argv[i]).substr(longArg ? 2 : 1);
      std::string nam = arg.substr(0, arg.find('='));
      if ((!longArg && nam == "h") || (longArg && nam == "help")) {
        Log::printf(LOG_INFO, "Usage");
        for (auto& [name, var] : consoleArguments) {
          Log::printf(LOG_INFO, "\t--%s=[%s]", name.c_str(),
                      var->getDefaultValue().c_str());
        }
        Log::printf(LOG_INFO, "\t-h, --help: shows help");
        exit(0);
      }

      if (longArg) {
        if (consoleArguments.find(nam) != consoleArguments.end()) {
          CVar* var = consoleArguments[nam];
          std::string value = arg.substr(arg.find('=') + 1);
          var->setValue(value);
        }
      }
    }
  }
}

void Settings::load() {
  FILE* sj = fopen(settingsPath.c_str(), "r");
  if (sj) {
    fseek(sj, 0, SEEK_END);
    int sjl = ftell(sj);
    fseek(sj, 0, SEEK_SET);
    char* sjc = (char*)malloc(sjl + 1);
    memset(sjc, 0, sjl + 1);
    fread(sjc, 1, sjl, sj);
    fclose(sj);

    try {
      json j = json::parse(sjc);
      p->oldSettings = j;
      json global = j["Global"];
      for (auto& cvar : global["CVars"].items()) {
        auto it = cvars.find(cvar.key());
        if (it != cvars.end()) {
          CVar* var = cvars[cvar.key()];
          if (var->getFlags() & CVARF_SAVE && var->getFlags() & CVARF_GLOBAL) {
            var->setValue(cvar.value());
          } else {
            throw std::runtime_error("");
          }
        }
      }
      json games = j["Games"];
      json game = games[Fun::getModuleName()];
      for (auto& cvar : game["CVars"].items()) {
        auto it = cvars.find(cvar.key());
        if (it != cvars.end()) {
          CVar* var = cvars[cvar.key()];
          if (var->getFlags() & CVARF_SAVE &&
              !(var->getFlags() & CVARF_GLOBAL)) {
            var->setValue(cvar.value());
          } else {
            throw std::runtime_error("");
          }
        }
      }
    } catch (std::exception& e) {
      Log::printf(LOG_ERROR, "Error parsing settings: %s", e.what());
    }

    free(sjc);
  } else {
    Log::printf(LOG_ERROR, "Couldn't open %s", settingsPath.c_str());
  }

  sj = fopen(settingsPrivatePath.c_str(), "r");
  if (sj) {
    fseek(sj, 0, SEEK_END);
    int sjl = ftell(sj);
    fseek(sj, 0, SEEK_SET);
    char* sjc = (char*)malloc(sjl + 1);
    memset(sjc, 0, sjl + 1);
    fread(sjc, 1, sjl, sj);
    fclose(sj);

    try {
      json j = json::parse(sjc);
      json global = j["data"];
      for (auto& cvar : global.items()) {
        auto it = cvars.find(cvar.key());
        if (it != cvars.end()) {
          CVar* var = cvars[cvar.key()];
          if (var->getFlags() & CVARF_SAVE && var->getFlags() & CVARF_GLOBAL &&
              var->getFlags() & CVARF_HIDDEN) {
            var->setValue(cvar.value());
          } else {
            throw std::runtime_error("");
          }
        }
      }
    } catch (std::exception& e) {
      Log::printf(LOG_ERROR, "Error parsing settings: %s", e.what());
    }

    free(sjc);
  } else {
    Log::printf(LOG_ERROR, "Couldn't open %s", settingsPrivatePath.c_str());
  }
}

std::vector<CVar*> Settings::getWithFlag(unsigned long mask, bool allowHidden) {
  std::vector<CVar*> var;
  for (auto cvar : cvars) {
    if (cvar.second->flags & CVARF_HIDDEN && !allowHidden) continue;
    if (cvar.second->flags & mask) var.push_back(cvar.second);
  }
  return var;
}

void Settings::save() {
  Log::printf(LOG_INFO, "writing settings to %s", settingsPath.c_str());
  FILE* sj = fopen(settingsPath.c_str(), "w");
  if (sj) {
    json j = p->oldSettings;
    json _cvars = {};
    json _cvars2 = {};
    for (auto cvar : cvars) {
      if (cvar.second->getFlags() & CVARF_HIDDEN &&
          cvar.second->getFlags() & CVARF_GLOBAL)
        continue;

      if (cvar.second->getFlags() & CVARF_SAVE &&
          cvar.second->getFlags() & CVARF_GLOBAL) {
        _cvars[cvar.first] = cvar.second->getValue();
      } else if (cvar.second->getFlags() & CVARF_SAVE) {
        _cvars2[cvar.first] = cvar.second->getValue();
      }
    }
    j["Global"]["CVars"] = _cvars;
    if (!_cvars2.is_null()) j["Games"][Fun::getModuleName()]["CVars"] = _cvars2;
    std::string d = j.dump(1);
    fwrite(d.data(), 1, d.size(), sj);
    fclose(sj);
  }

  FILE* sjp = fopen(settingsPrivatePath.c_str(), "w");
  if (sjp) {
    json j;
    j["warning"] =
        "DO not share this. Or edit this. Bad things will happen if you do";
    for (auto cvar : cvars) {
      if (cvar.second->getFlags() & CVARF_HIDDEN &&
          cvar.second->getFlags() & CVARF_GLOBAL &&
          cvar.second->getFlags() & CVARF_SAVE) {
        j["data"][cvar.second->getName()] = cvar.second->getValue();
      }
    }
    std::string d = j.dump(1);
    fwrite(d.data(), 1, d.size(), sjp);
    fclose(sjp);
  }
}
}  // namespace rdm
