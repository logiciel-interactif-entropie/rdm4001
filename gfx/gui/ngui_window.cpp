#include "ngui_window.hpp"

#include <cmath>
#include <cstdint>

#include "font.hpp"
#include "game.hpp"
#include "gfx/engine.hpp"
#include "input.hpp"
#include "ngui.hpp"
#include "world.hpp"
namespace rdm::gfx::gui {
NGuiElement::NGuiElement(NGuiManager* manager) {
  this->manager = manager;
  this->parent = NULL;
  size = glm::vec2(0, 0);
  minSize = glm::vec2(0, 0);
  maxSize = glm::vec2(INT32_MAX, INT32_MAX);
}

NGuiElement::~NGuiElement() {}

void NGuiVerticalLayout::layoutElements(NGuiPanel* panel,
                                        std::vector<NGuiElement*>& children) {
  glm::vec2 pos = glm::vec2(0);
  glm::vec2 posLocal = glm::vec2(getMargin());
  pos = panel->getPosition();
  if (panel->getParent() == NULL) pos.y += panel->getSize().y;
  pos.x += getMargin();
  pos.y -= getMargin();
  glm::vec2 max = glm::vec2(0);
  for (auto& child : children) {
    glm::vec2 maxSize = child->getMaxSize();
    maxSize.x = glm::max(maxSize.x, panel->getSize().x);
    child->setMaxSize(maxSize);
    child->setPosition(pos);

    if (NGuiPanel* panel = dynamic_cast<NGuiPanel*>(child)) {
      panel->doLayout();
    }

    pos.y -= child->getSize().y + getPadding();
    max = glm::max(child->getSize() + posLocal + glm::vec2(getMargin()), max);
    posLocal.y += child->getSize().y;
  }
  panel->setMinSize(max);
  panel->setSize(panel->getSize());
}

void NGuiHorizontalLayout::layoutElements(NGuiPanel* panel,
                                          std::vector<NGuiElement*>& children) {
  glm::vec2 pos = glm::vec2();
  glm::vec2 posLocal = glm::vec2(getMargin());
  pos = panel->getPosition();
  pos += getMargin();
  glm::vec2 max = glm::vec2(0);
  for (auto& child : children) {
    child->setPosition(pos);
    glm::vec2 maxSize = child->getMaxSize();
    maxSize.y = glm::max(maxSize.y, panel->getSize().y);
    child->setMaxSize(maxSize);

    if (NGuiPanel* panel = dynamic_cast<NGuiPanel*>(child)) {
      panel->doLayout();
    }

    pos.x += child->getSize().x + getPadding();
    max = glm::max(child->getSize() + posLocal + glm::vec2(getMargin()), max);
    posLocal.x += child->getSize().x;
  }
  panel->setMinSize(max);
  panel->setSize(panel->getSize());
}

void NGuiPanel::elementRender(NGuiRenderer* renderer) {
  for (auto child : children) {
    child->elementRender(renderer);
  }
}

void NGuiPanel::doLayout() { layout->layoutElements(this, children); }

glm::vec2 NGuiPanel::getContentSize() { glm::vec2 sz = getSize(); }

NGuiWindow::NGuiWindow(NGuiManager* gui, gfx::Engine* engine)
    : NGui(gui, engine), NGuiPanel(gui) {
  font = gui->getFontCache()->get("engine/gui/default.ttf", 14);
  titleFont = gui->getFontCache()->get("engine/gui/default.ttf", 11);
  closeButton = getResourceManager()->load<resource::Texture>(
      "engine/gui/close_button.png");

  visible = false;
  title = "";
  setPosition(glm::vec2(0));
  setMinSize(glm::vec2(200, 10));
  setMaxSize(glm::vec2(UINT32_MAX, UINT32_MAX));
  setSize(getMinSize());
}

void NGuiWindow::render(NGuiRenderer* renderer) {
  if (!visible) return;

  glm::vec2 size = getSize();
  glm::vec2 position = getPosition();

  renderer->setZIndex(UINT32_MAX);

  if (renderer->mouseDownZone(position + glm::vec2(0, size.y),
                              glm::vec2(size.x, 16)) == 1) {
    position += Input::singleton()->getMouseDelta() *
                Input::singleton()->getMouseSensitivity();
  }

  position = glm::max(glm::vec2(0), position);

  setPosition(position);

  renderer->setColor(glm::vec3(0.5));
  renderer->image(getEngine()->getWhiteTexture(), position, size);
  renderer->setColor(glm::vec3(0.4));
  renderer->image(getEngine()->getWhiteTexture(),
                  position + glm::vec2(0, size.y), glm::vec2(size.x, 16));

  renderer->setColor(glm::vec3(1.0));
  if (!title.empty()) {
    renderer->text(position + glm::vec2(5, size.y + 2.f), titleFont, 0, "%s",
                   title.c_str());
  }

  renderer->image(closeButton->getTexture(),
                  position + glm::vec2(size.x - 16.f, size.y),
                  glm::vec2(16.f, 16.f));
  if (renderer->mouseDownZone(position + glm::vec2(size.x - 16.f, size.y),
                              glm::vec2(16.f, 16.f)) == 1) {
    close();
  }

  frame();

  doLayout();
  elementRender(renderer);
}

void NGuiWindow::open() {
  glm::vec2 size = getSize();
  glm::vec2 res = getEngine()->getTargetResolution();
  setPosition(glm::vec2((res.x / 2.f) - (size.x / 2.f),
                        (res.y / 2.f) - (size.y / 2.f)));
  visible = true;
}

void NGuiWindow::close() {
  visible = false;
  closing();
}
};  // namespace rdm::gfx::gui
