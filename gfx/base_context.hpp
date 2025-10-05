#pragma once
#include <glm/glm.hpp>
#include <mutex>
#include <vector>

namespace rdm {
class AbstractionWindow;
};

namespace rdm::gfx {
/**
 * @brief The context, which is bound to a 'window' or an analagous widget.
 *
 * When using in threads other then Render, please lock using something like
 * `std::scoped_lock lock(context->getMutex());`
 */
class BaseContext {
  AbstractionWindow* hwnd;

  std::mutex mutex;

 public:
  BaseContext(AbstractionWindow* window);
  virtual ~BaseContext() {};

  struct DisplayMode {
    uint32_t format;
    int w, h;
    int refresh_rate;
    int display;
  };

  std::mutex& getMutex() { return mutex; }

  // YOU should lock mutex
  virtual void setCurrent() = 0;
  virtual void swapBuffers() = 0;
  // YOU should unlock mutex
  virtual void unsetCurrent() = 0;
  virtual glm::ivec2 getBufferSize() = 0;

  AbstractionWindow* getHwnd() { return hwnd; }
};
};  // namespace rdm::gfx
