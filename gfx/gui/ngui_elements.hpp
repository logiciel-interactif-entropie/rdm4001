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

 protected:
  std::string text;
  bool dirty;

 public:
  TextLabel(NGuiManager* manager) : NGuiElement(manager) {
    dirty = false;
    font = NULL;
    maxWidth = INT32_MAX;
    textTexture = manager->getEngine()->getDevice()->createTexture();
  };

  void setText(std::string text) {
    this->text = text;
    dirty = true;
  }

  void setFont(Font* font) {
    this->font = font;
    dirty = true;
  }

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
}  // namespace rdm::gfx::gui
