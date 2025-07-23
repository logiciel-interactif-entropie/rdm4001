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
  showThis = true;
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
    if (!child->getShowa()) continue;

    glm::vec2 maxSize = child->getMaxSize();
    maxSize.x = glm::max(maxSize.x, panel->getSize().x);
    child->setMaxSize(maxSize);
    child->setPosition(pos);

    if (NGuiPanel* panel = dynamic_cast<NGuiPanel*>(child)) {
      panel->doLayout();
    }

    pos.y -= child->getDisplaySize().y + getPadding();
    max = glm::max(child->getDisplaySize() + posLocal + glm::vec2(getMargin()),
                   max);
    posLocal.y += child->getDisplaySize().y + getPadding();
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
    if (!child->getShowa()) continue;

    child->setPosition(pos + glm::vec2(0.f, child->getSize().y));
    glm::vec2 maxSize = child->getMaxSize();
    maxSize.y = glm::max(maxSize.y, panel->getSize().y);
    child->setMaxSize(maxSize);

    if (NGuiPanel* panel = dynamic_cast<NGuiPanel*>(child)) {
      panel->doLayout();
    }

    pos.x += child->getSize().x + getPadding();
    max = glm::max(child->getSize() + posLocal + glm::vec2(getMargin()), max);
    posLocal.x += child->getSize().x + getPadding();
  }
  panel->setMinSize(max);
  panel->setSize(panel->getSize());
}

void NGuiPanel::elementRender(NGuiRenderer* renderer) {
  for (auto child : children) {
    if (child->getShowa()) child->elementRender(renderer);
  }
}

void NGuiPanel::doLayout() { layout->layoutElements(this, children); }

glm::vec2 NGuiPanel::getContentSize() { glm::vec2 sz = getSize(); }

NGuiWindow::NGuiWindow(NGuiManager* gui, gfx::Engine* engine)
    : NGui(gui, engine), NGuiPanel(gui) {
  font = gui->getFontCache()->get(NGUI_UI_FONT);
  titleFont = gui->getFontCache()->get(NGUI_TITLE_FONT);
  closeButton = getResourceManager()->load<resource::Texture>(
      "engine/gui/close_button.png");
  titleBar =
      getResourceManager()->load<resource::Texture>("engine/gui/title_bar.png");
  cornerRight =
      getResourceManager()->load<resource::Texture>("engine/gui/corner_r.png");
  cornerLeft =
      getResourceManager()->load<resource::Texture>("engine/gui/corner_l.png");
  horizBar =
      getResourceManager()->load<resource::Texture>("engine/gui/hbar.png");
  vertiLeftBar =
      getResourceManager()->load<resource::Texture>("engine/gui/vlbar.png");
  vertiRightBar =
      getResourceManager()->load<resource::Texture>("engine/gui/vrbar.png");

  visible = false;
  title = "";
  setPosition(glm::vec2(0));
  setMinSize(glm::vec2(200, 10));
  setMaxSize(glm::vec2(UINT32_MAX, UINT32_MAX));
  setSize(getMinSize());
  draggable = true;
  closable = true;
  hideDecorations = false;
}

void NGuiWindow::render(NGuiRenderer* renderer) {
  if (!visible) return;

  frame();

  glm::vec2 res = getEngine()->getTargetResolution();
  glm::vec2 size = getSize();
  glm::vec2 position = getPosition();

  renderer->setZIndex(UINT32_MAX);

  if (draggable) {
    if (renderer->mouseDownZone(position + glm::vec2(0, size.y),
                                glm::vec2(size.x - 16.f, 16)) == 1) {
      /*glm::vec2 delta = Input::singleton()->getMouseDelta();
      position += glm::vec2(delta.x, -delta.y) *
      Input::singleton()->getMouseSensitivity();*/
      position = Input::singleton()->getMousePosition();
      position.y = res.y - position.y - size.y - 10.f;
      position.x -= size.x / 2.f;
    }
  } else {
    if (putMeInTheCenter) {
      position = glm::floor((res / 2.f) - (getSize() / 2.f));
    }
  }

  position = glm::max(glm::vec2(5.f), position);
  position = glm::min(res - size - glm::vec2(5.f, 16.f), position);

  setPosition(position);

  if (!hideDecorations) {
    renderer->setColor(glm::vec3(0.255, 0.299, 0.365));
    renderer->image(getEngine()->getWhiteTexture(), position, size);
    renderer->setColor(glm::vec3(1.0));
    renderer->image(titleBar->getTexture(), position + glm::vec2(-5.f, size.y),
                    glm::vec2(size.x - (closable ? 11.f : -5.f) + 5.f, 16));
    renderer->image(horizBar->getTexture(), position + glm::vec2(11, -5),
                    glm::vec2(size.x - 22.f, 5));
    renderer->image(vertiRightBar->getTexture(), position + glm::vec2(-5, 0),
                    glm::vec2(5.f, size.y));
    renderer->image(vertiLeftBar->getTexture(), position + glm::vec2(size.x, 0),
                    glm::vec2(5.f, size.y));
    renderer->image(cornerLeft->getTexture(), position - glm::vec2(5),
                    glm::vec2(16, 16));
    renderer->image(cornerRight->getTexture(),
                    position + glm::vec2(size.x - 11.f, -5), glm::vec2(16, 16));

    renderer->setColor(glm::vec3(1.0));
    if (!title.empty()) {
      renderer->text(position + glm::vec2(5, size.y + 3.f), titleFont, 0, "%s",
                     title.c_str());
    }

    if (closable) {
      renderer->image(closeButton->getTexture(),
                      position + glm::vec2(size.x - 16.f + 5.f, size.y),
                      glm::vec2(16.f, 16.f));
      if (renderer->mouseDownZone(position + glm::vec2(size.x - 16.f, size.y),
                                  glm::vec2(16.f, 16.f)) == 1) {
        close();
      }
    }
  }

  doLayout();
  elementRender(renderer);
}

void NGuiWindow::open() {
  if (draggable) {
    glm::vec2 size = getSize();
    glm::vec2 res = getEngine()->getTargetResolution();
    setPosition(glm::vec2((res.x / 2.f) - (size.x / 2.f),
                          (res.y / 2.f) - (size.y / 2.f)));
  }
  visible = true;
  opening();
}

void NGuiWindow::close() {
  visible = false;
  closing();
}
};  // namespace rdm::gfx::gui
