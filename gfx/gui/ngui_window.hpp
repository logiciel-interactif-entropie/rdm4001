#pragma once
#include "ngui.hpp"
#include "resource.hpp"

#define NGUI_UI_FONT "engine/gui/msgothic.ttf", 14
#define NGUI_TITLE_FONT "engine/gui/msgothic.ttf", 11

namespace rdm::gfx::gui {
class NGuiStyle {};

class NGuiPanel;
class NGuiElement {
  friend class NGuiPanel;

  NGuiPanel* parent;
  NGuiManager* manager;
  glm::vec2 maxSize, minSize, size;
  glm::vec2 position;
  bool showThis;

 public:
  NGuiElement(NGuiManager* manager);
  virtual ~NGuiElement();

  void setMaxSize(glm::vec2 maxSize) { this->maxSize = maxSize; }
  void setMinSize(glm::vec2 minSize) { this->minSize = minSize; }
  void setSize(glm::vec2 size) {
    this->size = glm::min(maxSize, glm::max(minSize, size));
  }
  void setPosition(glm::vec2 position) { this->position = position; }
  void setShowa(bool show) { showThis = show; }

  glm::vec2 getMaxSize() { return maxSize; }
  glm::vec2 getMinSize() { return minSize; }
  glm::vec2 getSize() { return size; }
  glm::vec2 getDisplaySize() { return glm::max(minSize, size); }
  glm::vec2 getPosition() { return position; }
  NGuiPanel* getParent() { return parent; }
  NGuiManager* getGuiManager() { return manager; }
  bool getShowa() { return showThis; }

  virtual void elementRender(NGuiRenderer* renderer) {};
};

class NGuiLayout {
  friend class NGuiPanel;
  virtual void layoutElements(NGuiPanel* panel,
                              std::vector<NGuiElement*>& children) = 0;

  int padding, margin;

 public:
  NGuiLayout() {
    padding = 2;
    margin = 2;
  }

  void setPadding(int padding) { this->padding = padding; };
  void setMargin(int margin) { this->margin = margin; };

  int getPadding() { return padding; }
  int getMargin() { return margin; }
};

class NGuiVerticalLayout : public NGuiLayout {
  virtual void layoutElements(NGuiPanel* panel,
                              std::vector<NGuiElement*>& children);

 public:
};

class NGuiHorizontalLayout : public NGuiLayout {
  virtual void layoutElements(NGuiPanel* panel,
                              std::vector<NGuiElement*>& children);

 public:
};

class NGuiPanel : public NGuiElement {
  std::unique_ptr<NGuiLayout> layout;
  std::vector<NGuiElement*> children;

 public:
  NGuiPanel(NGuiManager* manager)
      : NGuiElement(manager), layout(new NGuiVerticalLayout()) {}

  glm::vec2 getContentSize();

  void doLayout();

  void addElement(NGuiElement* element) {
    element->parent = this;
    children.push_back(element);
  };

  void setLayout(NGuiLayout* layout) { this->layout.reset(layout); };
  NGuiLayout* getLayout() { return layout.get(); }

  virtual void elementRender(NGuiRenderer* renderer);
};

class NGuiWindow : public NGui, public NGuiPanel {
  std::string title;
  Font* font;
  Font* titleFont;
  resource::Texture* closeButton;
  resource::Texture* titleBar;
  resource::Texture* cornerRight;
  resource::Texture* cornerLeft;
  resource::Texture* horizBar;
  resource::Texture* vertiLeftBar;
  resource::Texture* vertiRightBar;
  bool draggable;
  bool closable;
  bool visible;
  bool putMeInTheCenter;
  bool hideDecorations;

 public:
  NGuiWindow(NGuiManager* gui, gfx::Engine* engine);

  void setTitle(std::string title) { this->title = title; };
  void setClosable(bool closable) { this->closable = closable; }
  void setDraggable(bool draggable) { this->draggable = draggable; }
  void setCenter(bool center) { this->putMeInTheCenter = center; }
  void setHideDecorations(bool hide) { hideDecorations = hide; }

  void open();
  void close();
  bool isVisible() { return visible; }

  virtual void opening() {};
  virtual void closing() {};
  virtual void frame() {};
  virtual void render(NGuiRenderer* renderer);
};
}  // namespace rdm::gfx::gui
