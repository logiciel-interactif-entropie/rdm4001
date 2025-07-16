#include "ngui_elements.hpp"

#include "font.hpp"
#include "ngui.hpp"

#define UI_FONT "engine/gui/default.ttf", 14

namespace rdm::gfx::gui {
void TextLabel::updateText() {
  if (!dirty) return;
  dirty = false;

  Font* toUse = font;
  if (!toUse) {
    toUse = getGuiManager()->getFontCache()->get(UI_FONT);
  }

  OutFontTexture t = FontRender::renderWrapped(
      toUse, text.size() ? text.c_str() : " ",
      std::min(maxWidth, (unsigned int)getMaxSize().x));
  textTexture->upload2d(t.w, t.h, DtUnsignedByte, BaseTexture::RGBA, t.data, 0);
  setMinSize(glm::vec2(t.w, t.h));
  setSize(getSize());
}

void TextLabel::elementRender(NGuiRenderer* renderer) {
  updateText();

  renderer->setColor(glm::vec3(1.0));
  glm::vec2 p = getPosition();
  p.y -= getMinSize().y;
  renderer->image(textTexture.get(), p, getMinSize());
}

void TextInput::elementRender(NGuiRenderer* renderer) {
  if (text != (prefix + textData)) dirty = true;
  if (std::string(textData).empty() && !emptyText.empty()) {
    text = emptyText;
  } else {
    text = prefix + textData;
  }

  updateText();

  glm::vec2 p = getPosition();
  p.y -= getMinSize().y;

  int status = renderer->mouseDownZone(p, getSize());
  bool value = false;
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
      value = true;
      break;
  }

  if (value) {
    if (!debounce) {
      getGuiManager()->setCurrentText(textData, 65535);
      debounce = true;
    }
  } else {
    if (status == -2 && getGuiManager()->isCurrentText(textData)) {
      getGuiManager()->setCurrentText(NULL, 0);
    }

    if (debounce) {
      debounce = false;
    }
  }

  renderer->image(getGuiManager()->getEngine()->getWhiteTexture(), p,
                  getMinSize());
  TextLabel::elementRender(renderer);
}

void Button::elementRender(NGuiRenderer* renderer) {
  updateText();

  glm::vec2 p = getPosition();
  p.y -= getMinSize().y;
  renderer->setColor(glm::vec3(0.5));
  int status = renderer->mouseDownZone(p, getSize());
  bool value = false;
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
      value = true;
      break;
  }

  if (value) {
    if (!debounce) {
      if (pressed.has_value()) pressed.value()();
      debounce = true;
    }
  } else {
    if (debounce) {
      debounce = false;
    }
  }

  renderer->image(getGuiManager()->getEngine()->getWhiteTexture(), p,
                  getMinSize());

  TextLabel::elementRender(renderer);
}
}  // namespace rdm::gfx::gui
