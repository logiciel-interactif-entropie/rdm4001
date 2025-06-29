#include "fun.hpp"

#include <stdio.h>
#include <stdlib.h>

#include <format>
#include <fstream>
#include <iostream>

#include "filesystem.hpp"
#include "logging.hpp"
#include "settings.hpp"

#ifdef __linux
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __HAIKU__
#include <FindDirectory.h>
#include <app/Application.h>
#endif

#include <string.h>

namespace rdm {
bool Fun::preFlightChecks() {
#ifdef __linux
  FILE* fp = fopen("/proc/sys/fs/binfmt_misc/WSLInterop", "r");
  if (fp) {
    printf(
        "Your graphics card is not supported by RDM4001, and thus it cannot "
        "start. Sorry\n");
    return false;
  }
#endif
  return true;  // OK
}

static CVar cl_username("cl_username", "", CVARF_GLOBAL | CVARF_SAVE);

std::string Fun::getLocalDataDirectory() {
#if defined(__linux)
  struct passwd* pw = getpwuid(getuid());
  std::string home = pw->pw_dir;
  std::string path = home + "/.local/share/rdm4001/";
  DIR* configDir = opendir(path.c_str());
  if (configDir) {
    // yay
  } else if (ENOENT == errno) {
    mkdir((home + "/.local").c_str(), 0777);
    mkdir((home + "/.local/share").c_str(), 0777);
    if (mkdir(path.c_str(), 0777)) {
      Log::printf(LOG_FATAL, "creating data dir error %s", strerror(errno));
      throw std::runtime_error("getLocalDataDirectory");
    }
    mkdir((path + "hosts").c_str(), 0700);
  } else {
    Log::printf(LOG_FATAL, "opening data dir error %s", strerror(errno));
    throw std::runtime_error("getLocalDataDirectory");
  }
  return path;
#elif defined(__HAIKU__)

#else
#error getLocalDataDirectory unimplemented on current platform
#endif
}

std::string Fun::getSystemUsername() {
  std::string username;
  if (cl_username.getValue().empty()) {
#ifdef __linux
    char* _username = getenv("USER");
    username = _username;
#else
    // do it for windows
    username = std::format("User{}", rand() % 99999);
#endif
    cl_username.setValue(username);
  } else {
    username = cl_username.getValue();
  }
  return username;
}

std::string Fun::getModuleName() {
#if defined(PLATFORM_POSIX) || defined(__linux__)
  std::string sp;
  std::ifstream("/proc/self/comm") >> sp;
  return sp;
#elif defined(_WIN32)
  char buf[MAX_PATH];
  GetModuleFileNameA(nullptr, buf, MAX_PATH);
  return buf;
#elif defined(__HAIKU__)
  return "Generic";
#else
  static_assert(false, "unrecognized platform");
#endif
}
}  // namespace rdm
