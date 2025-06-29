#include "input.hpp"

#include <pthread.h>

#include <csignal>
#include <thread>

#include "SDL.h"
#include "SDL_keyboard.h"
#include "logging.hpp"
#include "settings.hpp"

#ifdef __linux
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace rdm {
#ifdef __linux
static void intHandler(int) {
  Log::printf(LOG_INFO, "Received SIGINT, sending Quit signal");

  InputObject quit;
  quit.type = InputObject::Quit;
  Input::singleton()->postEvent(quit);
}

extern std::map<std::thread::id, std::string> __threadNames;

static void sigHandler(int) {
  std::string threadName = "Unknown";
  if (__threadNames.find(std::this_thread::get_id()) != __threadNames.end())
    threadName = __threadNames[std::this_thread::get_id()];

  Log::printf(LOG_FATAL, "Received SIGSEGV on thread %s", threadName.c_str());
#ifndef NDEBUG
  bool allowed = false;
  FILE* perm = fopen("/proc/sys/kernel/yama/ptrace_scope", "r");
  char pms = 'X';
  fread(&pms, 1, 1, perm);
  switch (pms) {
    case '0':
      allowed = true;
      break;
    default:
      Log::printf(LOG_ERROR,
                  "/proc/sys/kernel/yama/ptrace_scope returns %c, please set "
                  "it to 0 in order to use automatic gdb attach",
                  pms);
      break;
  }

  if (allowed) {
    char name[128];
    pthread_getname_np(pthread_self(), name, 128);
    pthread_setname_np(pthread_self(),
                       (name + std::string("[SIGSEGV]")).c_str());

    const char* path = getenv("PATH");
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, ':')) {
      std::string path = (item + "/gdb");
      FILE* gdb = fopen(path.c_str(), "r");
      if (gdb) {
        std::string pids = std::to_string(getpid());
        std::string tids = std::to_string(pthread_self());
        std::string exec =
            path + " -q -p " + pids + " -ex 'thread " + tids + "'";
        Log::printf(LOG_INFO, "Starting gdb `%s`", exec.c_str());
        system(exec.c_str());
        break;
      }
    }
  }
#endif
  Log::printf(LOG_FATAL, "Emergency exit, nothing will be saved");
  exit(-1);
}
#endif

Input::Input() {
  mouseLocked = false;
  mouseSensitivity = 2.0;
  mouseDelta = glm::vec3(0);
  mousePosition = glm::vec3(0);

  editingText = false;
  memset(keysDown, 0, sizeof(keysDown));
  memset(mouseButtonsDown, 0, sizeof(mouseButtonsDown));

#ifdef __linux
  signal(SIGINT, intHandler);
  signal(SIGSEGV, sigHandler);
#endif
}

rdm::Input* _singleton = 0;
rdm::Input* Input::singleton() {
  if (!_singleton) {
    _singleton = new rdm::Input();
  }
  return _singleton;
}

void Input::startEditingText(bool clear) {
  if (clear) text.clear();
  SDL_StartTextInput();
  editingText = true;
}

void Input::stopEditingText() {
  SDL_StopTextInput();
  editingText = false;
}

std::string& Input::getEditedText() { return text; }

void Input::beginFrame() { mouseDelta = glm::vec3(0.0); }

void Input::postEvent(InputObject object) {
  events.push_back(object);

  if (events.size() > 1000)
    Log::printf(LOG_WARN, "Too many events in event buffer (%i)",
                events.size());
}

void Input::flushEvents() {
  std::scoped_lock lock(flushing);
  while (events.size()) {
    InputObject event = events.front();
    bool keyPressed = false;
    switch (event.type) {
      case InputObject::Quit:
        quitSignal.fire(event);
        break;
      case InputObject::KeyPress:
        keyDownSignals[event.data.key.key].fire();
      case InputObject::KeyUp:
        keyPressed = event.type == InputObject::KeyPress;
        /*for (auto [name, _axis] : axis) {
          if (_axis.positive == event.data.key.key) {
            if (keyPressed) {
              if (_axis.value == 0.0) _axis.value = 1.0;
            } else {
              if (_axis.value == 1.0) _axis.value = 0.0;
            }
          } else if (_axis.negative == event.data.key.key) {
            if (keyPressed) {
              if (_axis.value == 0.0) _axis.value = -1.0;
            } else {
              if (_axis.value == -1.0) _axis.value = 0.0;
            }
          }
          axis[name] = _axis;
          }*/
        if (event.data.key.key < 255) {
          keysDown[event.data.key.key] = keyPressed;
        }
        break;
      case InputObject::MousePress:
        mousePressed.fire();
      case InputObject::MouseUp:
        keyPressed = event.type == InputObject::MousePress;
        mouseButtonsDown[event.data.mouse.button] = keyPressed;
        break;
      case InputObject::MouseMove:
        mousePosition.x = event.data.mouse.position[0];
        mousePosition.y = event.data.mouse.position[1];
        mousePosition *= Settings::singleton()->getCvar("r_scale")->getFloat();
        mouseDelta.x += ((float)event.data.mouse.delta[0]) / mouseSensitivity;
        mouseDelta.y += ((float)event.data.mouse.delta[1]) / mouseSensitivity;
        break;
      default:
        break;
    }

    onEvent.fire(event);
    events.pop_front();
  }

  for (auto& [name, _axis] : axis) {
    float v = 0.f;
    if (keysDown[_axis.positive]) v += 1.f;
    if (keysDown[_axis.negative]) v -= 1.f;
    _axis.value = v;
  }
}

Input::Axis* Input::newAxis(std::string _axis, SDL_Keycode positive,
                            SDL_Keycode negative) {
  Axis x;
  x.value = 0.0;
  x.negative = negative;
  x.positive = positive;
  axis[_axis] = x;
  return &axis[_axis];
}

Input::Axis* Input::getAxis(std::string _axis) { return &axis[_axis]; }
}  // namespace rdm
