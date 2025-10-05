#include "logging.hpp"

#include <stdarg.h>
#include <stdio.h>

#include <chrono>

namespace rdm {
Log::Log() { level = LOG_DEBUG; }

static Log* _singleton = 0;
Log* Log::singleton() {
  if (!_singleton) _singleton = new Log();
  return _singleton;
}

#ifdef NDEBUG
void Log::print(LogType t, const char* s) {
  LogMessage m;
  m.t = t;
  m.message = std::string(s);
  m.time = std::chrono::steady_clock::now();
  singleton()->addLogMessage(m);
}
#else
void Log::print(LogType t, const char* s, const std::source_location loc) {
  LogMessage m;
  m.t = t;
  m.message = std::string(s);
  m.loc = loc;
  m.time = std::chrono::steady_clock::now();
  singleton()->addLogMessage(m);
}
#endif

void Log::addLogMessage(LogMessage m) {
  std::scoped_lock lock(mutex);
  if (m.t >= level) {
    char* clr = "";
    char* lvl = "";
    switch (m.t) {
      default:
      case LOG_EXTERNAL:
      case LOG_DEBUG:
        lvl = "DEBUG";
        break;
      case LOG_FIXME:
        clr = YEL;
        lvl = "FIXME";
        break;
      case LOG_INFO:
        lvl = "INFO";
        break;
      case LOG_WARN:
        clr = YEL;
        lvl = "WARNING";
        break;
      case LOG_ERROR:
        clr = RED;
        lvl = "ERROR";
        break;
      case LOG_FATAL:
        clr = BRED;
        lvl = "FATAL";
        break;
    }
#ifndef _WIN32
#ifndef NDEBUG
    ::printf("%s%s:%i %s: %s" COLOR_RESET "\n", clr, m.loc.file_name(),
             m.loc.line(), lvl, m.message.c_str());
#else
    ::printf("%s%s" COLOR_RESET "\n", clr, m.message.c_str());
#endif
#else
#ifndef NDEBUG
    ::printf("%s:%i %s: %s\n", m.loc.file_name(), m.loc.line(), lvl,
             m.message.c_str());
#else
    ::printf("%s\n", clr, m.message.c_str());
#endif
#endif
  }

  log.push_front(m);

  if (log.size() > 2000) {
    log.pop_back();
  }
}
}  // namespace rdm
