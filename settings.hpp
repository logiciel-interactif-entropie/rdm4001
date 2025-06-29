#pragma once
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <variant>

#include "signal.hpp"

namespace rdm {
typedef std::variant<std::string, int, float> Setting;

#define CVARF_REPLICATE (1 << 0)
#define CVARF_NOTIFY (1 << 1)
#define CVARF_SAVE (1 << 2)

/**
 * @brief This is only for CVar's defined by RDM4001. Try not to use this in
 * your project
 */
#define CVARF_GLOBAL (1 << 3)
#define CVARF_HIDDEN (1 << 4)

#define CVARF_CONSOLE_ARGUMENT (1 << 5)

class CVar {
  friend class Settings;

  std::string name;
  std::string value;
  std::string defaultVar;
  unsigned long flags;
  bool dirty;

 public:
  CVar(const char* name, const char* defaultVar, unsigned long flags = 0);
  std::string getName() { return name; }
  std::string getValue() { return value; }
  std::string getDefaultValue() { return defaultVar; }
  void setValue(std::string s);

  unsigned long getFlags() { return flags; }

  Signal<> changing;

  int getInt();
  void setInt(int i);

  float getFloat();
  void setFloat(float f);

  glm::vec2 getVec2();
  void setVec2(glm::vec2 v);

  glm::vec3 getVec3();
  void setVec3(glm::vec3 v);

  glm::vec4 getVec4(int ms = 4);
  void setVec4(glm::vec4 v);

  bool getBool();
  void setBool(bool b);
};

struct SettingsPrivate;

class Settings {
  friend class CVar;

  Settings();

  bool hintDs;

  SettingsPrivate* p;
  std::map<std::string, CVar*> cvars;
  std::map<std::string, CVar*> consoleArguments;
  std::string settingsPath;
  std::string settingsPrivatePath;
  std::string gamePath;
  std::string hintConnect;
  int hintConnectPort;

  void addCvar(const char* name, CVar* cvar) {
    if (cvar->getFlags() & CVARF_CONSOLE_ARGUMENT)
      consoleArguments[name] = cvar;
    else
      cvars[name] = cvar;
  };

 public:
  static Settings* singleton();

  CVar* getCvar(const char* name, bool allowHidden = false) {
    auto it = cvars.find(name);
    if (it != cvars.end()) {
      if (cvars[name]->flags & CVARF_HIDDEN && !allowHidden) return NULL;
      return cvars[name];
    }
    return NULL;
  };

  void parseCommandLine(char* argv[], int argc);
  void load();
  void save();

  std::vector<CVar*> getWithFlag(unsigned long mask = UINT64_MAX,
                                 bool allowHidden = false);

  void listCvars();

  Signal<std::string> cvarChanging;

  std::string getGamePath() { return gamePath; }

  bool getHintDs() { return hintDs; }
  std::string getHintConnectIP() { return hintConnect; }
  int getHintConnectPort() { return hintConnectPort; }
};
}  // namespace rdm
