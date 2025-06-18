#pragma once
#include <SDL2/SDL_keycode.h>

#include <deque>
#include <glm/glm.hpp>
#include <map>
#include <mutex>
#include <string>

#include "signal.hpp"

namespace rdm {
/**
 * @brief This is the object passed around to signal input events
 */
struct InputObject {
  enum Type {
    KeyPress,
    KeyUp,
    MouseMove,
    MousePress,
    MouseUp,
    MouseScroll,
    Quit,
    TextEdit,
  };

  Type type;
  union {
    struct {
      SDL_Keycode key;
      char sym;
      SDL_Keymod mod;
    } key;
    struct {
      int delta[2];
      int position[2];
      int global_position[2];
      int button;
      bool mouse_down;
      int wheel;
    } mouse;
  } data;
};

class Input {
  Input();

  std::mutex flushing;
  glm::vec2 mouseDelta;
  glm::vec2 mousePosition;
  bool mouseButtonsDown[10];
  std::deque<InputObject> events;
  bool mouseLocked;
  bool editingText;
  std::string text;
  float mouseSensitivity;

  bool keysDown[255];

 public:
  static Input* singleton();

  bool isEditingText() { return editingText; }
  void startEditingText(bool clear = true);
  void stopEditingText();
  std::string& getEditedText();

  void beginFrame();
  void postEvent(InputObject object);
  void flushEvents();

  /**
   * @brief This is a struct that will automatically be updated every
   * flushEvents(), which happens on scheduler 0's world tick
   */
  struct Axis {
    float value;
    SDL_Keycode positive;
    SDL_Keycode negative;
  };

  glm::vec2 getMouseDelta() {
    glm::vec2 d = mouseDelta;
    mouseDelta = glm::vec2(0);
    return d;
  };
  glm::vec2 getMousePosition() { return mousePosition; };

  Axis* newAxis(std::string axis, SDL_Keycode positive, SDL_Keycode negative);
  Axis* getAxis(std::string axis);

  Signal<> mousePressed;
  Signal<> mouseReleased;

  void setMouseLocked(bool b) { mouseLocked = b; }
  bool getMouseLocked() { return mouseLocked; }

  Signal<InputObject> quitSignal;
  Signal<InputObject> onEvent;

  bool isKeyDown(int c) { return keysDown[c]; };
  bool isMouseButtonDown(int i) { return mouseButtonsDown[i]; }

  std::map<int, Signal<>> keyDownSignals;

 private:
  std::map<std::string, Axis> axis;
};
}  // namespace rdm
