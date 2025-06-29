#include "ngui_window.hpp"

#include <cstdint>

#include "font.hpp"
#include "game.hpp"
#include "gfx/engine.hpp"
#include "input.hpp"
#include "world.hpp"
namespace rdm::gfx::gui {
NGuiWindow::NGuiWindow(NGuiManager* gui, gfx::Engine* engine)
    : NGui(gui, engine) {
  font = gui->getFontCache()->get("engine/gui/default.ttf", 11);
  closeButton = engine->getWorld()
                    ->getGame()
                    ->getResourceManager()
                    ->load<resource::Texture>("engine/gui/close_button.png");

  visible = false;
  title = "";
  position = glm::vec2(0);
  setMinSize(glm::vec2(200, 300));
  setMaxSize(glm::vec2(UINT32_MAX, UINT32_MAX));
  setSize(minSize);
}

NGuiWindow::Render::Render(NGuiRenderer* renderer, NGuiWindow* window,
                           glm::vec2 offset) {
  this->renderer = renderer;
  this->window = window;
  font = window->font;
  engine = window->getEngine();

  elemPos = window->getPosition() +
            glm::vec2(offset.x, window->getSize().y - offset.y);
}

void NGuiWindow::Render::text(const char* text, ...) {
  va_list ap;
  va_start(ap, text);
  char buf[65535];
  vsnprintf(buf, sizeof(buf), text, ap);
  renderer->setColor(glm::vec3(1.0));
  auto p = renderer->text(elemPos, font, window->getSize().x - 15.f, "%s", buf);
  renderer->getLastCommand()->setOffset(
      glm::vec2(elemPos.x, elemPos.y - p.second));
  elemPos.y -= p.second;
  va_end(ap);
}

bool NGuiWindow::Render::button(const char* text) {
  glm::ivec2 bdim = font->getTextSize(text);
  int status = renderer->mouseDownZone(elemPos, bdim);

  switch (status) {
    default:
    case -1:
      renderer->setColor(glm::vec3(0.2));
      break;
    case 0:
      renderer->setColor(glm::vec3(0.0));
      break;
    case 1:
      renderer->setColor(glm::vec3(1.0));
      break;
  }
  renderer->image(engine->getWhiteTexture(), elemPos, bdim);
  renderer->setColor(glm::vec3(1.0));
  renderer->text(elemPos, font, 0, text);
  elemPos.y -= bdim.x;
  return status;
}

void NGuiWindow::render(NGuiRenderer* renderer) {
  if (!visible) return;

  renderer->setZIndex(UINT32_MAX);

  if (renderer->mouseDownZone(position + glm::vec2(0, size.y),
                              glm::vec2(size.x, 16)) == 1) {
    position += Input::singleton()->getMouseDelta() *
                Input::singleton()->getMouseSensitivity();
  }

  position = glm::max(glm::vec2(0), position);

  renderer->setColor(glm::vec3(0.5));
  renderer->image(getEngine()->getWhiteTexture(), position, size);
  renderer->setColor(glm::vec3(0.4));
  renderer->image(getEngine()->getWhiteTexture(),
                  position + glm::vec2(0, size.y), glm::vec2(size.x, 16));

  renderer->setColor(glm::vec3(1.0));
  if (!title.empty()) {
    renderer->text(position + glm::vec2(5, size.y + 2.f), font, 0, "%s",
                   title.c_str());
  }

  renderer->image(closeButton->getTexture(),
                  position + glm::vec2(size.x - 16.f, size.y),
                  glm::vec2(16.f, 16.f));
  if (renderer->mouseDownZone(position + glm::vec2(size.x - 16.f, size.y),
                              glm::vec2(16.f, 16.f)) == 1) {
    visible = false;
  }

  Render render(renderer, this, glm::vec2(15, 15));
  show(&render);
}

void NGuiWindow::open() {
  glm::vec2 res = getEngine()->getTargetResolution();
  position =
      glm::vec2((res.x / 2.f) - (size.x / 2.f), (res.y / 2.f) - (size.y / 2.f));
  visible = true;
}
};  // namespace rdm::gfx::gui
