#pragma once
#include <chrono>
#include <deque>
#include <mutex>
#include <source_location>
#include <string>

// Regular text
#define BLK "\e[0;30m"
#define RED "\e[0;31m"
#define GRN "\e[0;32m"
#define YEL "\e[0;33m"
#define BLU "\e[0;34m"
#define MAG "\e[0;35m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"

// Regular bold text
#define BBLK "\e[1;30m"
#define BRED "\e[1;31m"
#define BGRN "\e[1;32m"
#define BYEL "\e[1;33m"
#define BBLU "\e[1;34m"
#define BMAG "\e[1;35m"
#define BCYN "\e[1;36m"
#define BWHT "\e[1;37m"

// Regular underline text
#define UBLK "\e[4;30m"
#define URED "\e[4;31m"
#define UGRN "\e[4;32m"
#define UYEL "\e[4;33m"
#define UBLU "\e[4;34m"
#define UMAG "\e[4;35m"
#define UCYN "\e[4;36m"
#define UWHT "\e[4;37m"

// Regular background
#define BLKB "\e[40m"
#define REDB "\e[41m"
#define GRNB "\e[42m"
#define YELB "\e[43m"
#define BLUB "\e[44m"
#define MAGB "\e[45m"
#define CYNB "\e[46m"
#define WHTB "\e[47m"

// High intensty background
#define BLKHB "\e[0;100m"
#define REDHB "\e[0;101m"
#define GRNHB "\e[0;102m"
#define YELHB "\e[0;103m"
#define BLUHB "\e[0;104m"
#define MAGHB "\e[0;105m"
#define CYNHB "\e[0;106m"
#define WHTHB "\e[0;107m"

// High intensty text
#define HBLK "\e[0;90m"
#define HRED "\e[0;91m"
#define HGRN "\e[0;92m"
#define HYEL "\e[0;93m"
#define HBLU "\e[0;94m"
#define HMAG "\e[0;95m"
#define HCYN "\e[0;96m"
#define HWHT "\e[0;97m"

// Bold high intensity text
#define BHBLK "\e[1;90m"
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHYEL "\e[1;93m"
#define BHBLU "\e[1;94m"
#define BHMAG "\e[1;95m"
#define BHCYN "\e[1;96m"
#define BHWHT "\e[1;97m"

// Reset
#define COLOR_RESET "\e[0m"

namespace rdm {
enum LogType {
  LOG_EXTERNAL,
  LOG_DEBUG,
  LOG_FIXME,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_FATAL
};

struct LogMessage {
  LogType t;
  std::string message;
#ifndef NDEBUG
  std::source_location loc;
#endif
  std::chrono::time_point<std::chrono::steady_clock> time;
};

class Log {
  std::deque<LogMessage> log;
  std::mutex mutex;
  LogType level;

  Log();

 public:
  static Log* singleton();

  const std::deque<LogMessage>& getLogMessages() { return log; }

#ifdef NDEBUG
  template <typename... Args>
  static void printf(LogType type, const char* f, Args&&... args) {
    char buf[4096];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    snprintf(buf, sizeof(buf), f, args...);
    print(type, buf);
#pragma GCC diagnostic pop
  };
  static void print(LogType type, const char* f);
#else
  template <typename T, typename S, typename... Args>
  struct printf {
    printf(T t, S s, Args&&... args,
           const std::source_location& loc = std::source_location::current()) {
      char buf[4096];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
      snprintf(buf, sizeof(buf), s, args...);
      print(t, buf, loc);
#pragma GCC diagnostic pop
    }
  };

  template <typename... Args>
  printf(LogType type, const char* f, Args&&... args)
      -> printf<LogType, const char*, Args...>;
  static void print(
      LogType type, const char* f,
      const std::source_location loc = std::source_location::current());
#endif

  void setLevel(LogType level) { this->level = level; };
  void addLogMessage(LogMessage m);
};
}  // namespace rdm
