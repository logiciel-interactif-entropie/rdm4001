#include <stdint.h>

#include <functional>

#include "gfx/base_types.hpp"
#include "ngui.hpp"
#include "ngui_window.hpp"

namespace rdm::gfx::gui {
class TextLabel : public NGuiElement {
  std::unique_ptr<BaseTexture> textTexture;
  Font* font;
  unsigned int maxWidth;
  bool autowrap;
  glm::vec3 color;

 protected:
  std::string text;
  bool dirty;

 public:
  TextLabel(NGuiManager* manager) : NGuiElement(manager) {
    dirty = true;
    font = NULL;
    maxWidth = INT32_MAX;
    textTexture = manager->getEngine()->getDevice()->createTexture();
    autowrap = false;
  };

  void setAutoWrap(bool wrap) { autowrap = wrap; }

  void setText(std::string text) {
    this->text = text;
    dirty = true;
  }

  void setFont(Font* font) {
    this->font = font;
    dirty = true;
  }

  void setColor(glm::vec3 color) { this->color = color; }

  void setTextMaxWidth(int maxWidth) {
    this->maxWidth = maxWidth;
    dirty = true;
  }

  void updateText();

  virtual void elementRender(NGuiRenderer* renderer);
};

class TextInput : public TextLabel {
  std::string prefix;
  std::string emptyText;
  bool debounce;
  char textData[65535];

 public:
  TextInput(NGuiManager* manager) : TextLabel(manager) {
    memset(textData, 0, 65535);
    setMaxSize(glm::vec2(INT32_MAX, 0));
  }

  std::string getLine() { return textData; }
  void setLine(std::string ln) { strncpy(textData, ln.data(), 65535); }
  void setEmptyText(std::string emptyText) { this->emptyText = emptyText; }
  void setPrefix(std::string prefix) { this->prefix = prefix; }

  virtual void elementRender(NGuiRenderer* renderer);
};

class Button : public TextLabel {
  bool debounce;
  std::optional<std::function<void()>> pressed;

 public:
  Button(NGuiManager* manager) : TextLabel(manager) { debounce = false; };

  void setPressed(std::function<void()> pressed) { this->pressed = pressed; };

  virtual void elementRender(NGuiRenderer* renderer);
};

class Table : public NGuiElement {
  int rows, columns;

 public:
  Table(NGuiManager* manager) : NGuiElement(manager) {
    rows = 2;
    columns = 2;
  };
};

class Image : public NGuiElement {
  gfx::BaseTexture* texture;

 public:
  Image(NGuiManager* manager) : NGuiElement(manager) {
    texture = NULL;
    setMinSize(glm::vec2(128));
  };

  void setTexture(resource::Texture* image) {
    texture = image->getTexture();
    setMinSize(glm::vec2(image->getWidth(), image->getHeight()));
  };
  void setTexture(gfx::BaseTexture* texture) { this->texture = texture; }

  virtual void elementRender(NGuiRenderer* renderer);
};
}  // namespace rdm::gfx::gui
