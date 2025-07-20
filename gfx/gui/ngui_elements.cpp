#include "ngui_elements.hpp"

#include "font.hpp"
#include "ngui.hpp"

namespace rdm::gfx::gui {
void TextLabel::updateText() {
  if (!dirty) return;
  dirty = false;

  Font* toUse = font;
  if (!toUse) {
    toUse = getGuiManager()->getFontCache()->get(NGUI_UI_FONT);
  }

  OutFontTexture t = FontRender::renderWrapped(
      toUse, text.size() ? text.c_str() : " ",
      autowrap ? std::min(maxWidth, (unsigned int)getMaxSize().x) : 0);
  textTexture->upload2d(t.w, t.h, DtUnsignedByte, BaseTexture::RGBA, t.data, 0);
  setMinSize(glm::vec2(t.w, t.h));
  setSize(getSize());
  color = glm::vec3(1.0);
}

void TextLabel::elementRender(NGuiRenderer* renderer) {
  updateText();

  renderer->setColor(color);
  glm::vec2 p = getPosition();
  p.y -= getMinSize().y;
  renderer->image(textTexture.get(), p, getMinSize());
}

void TextInput::elementRender(NGuiRenderer* renderer) {
  std::string newText;
  if (std::string(textData).empty() && !emptyText.empty()) {
    newText = emptyText;
    setColor(glm::vec3(0.5));
  } else {
    newText = prefix + textData;
    setColor(glm::vec3(1.0));
  }
  if (text != newText) {
    dirty = true;
    text = newText;
  }

  updateText();

  setSize(getSize());

  glm::vec2 p = getPosition();
  p.y -= getMinSize().y;

  int status = renderer->mouseDownZone(p, getDisplaySize());
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

  renderer->image(getGuiManager()->getEngine()->getWhiteTexture(), p,
                  getDisplaySize());

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

  setSize(getSize());
  TextLabel::elementRender(renderer);
}

void Image::elementRender(NGuiRenderer* renderer) {
  glm::vec2 p = getPosition();
  p.y -= getSize().y;
  if (texture) {
    renderer->image(texture, p, getSize());
  }
}
}  // namespace rdm::gfx::gui
