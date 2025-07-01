#pragma once
#include "ngui.hpp"
#include "resource.hpp"

namespace rdm::gfx::gui {
class NGuiWindow : public NGui {
  glm::vec2 maxSize, minSize, size;
  glm::vec2 position;
  std::string title;
  Font* font;
  Font* titleFont;
  resource::Texture* closeButton;
  bool visible;

 public:
  NGuiWindow(NGuiManager* gui, gfx::Engine* engine);

  class Render {
    friend class NGuiWindow;
    NGuiRenderer* renderer;
    NGuiWindow* window;
    gfx::Engine* engine;
    Font* font;

    Render(NGuiRenderer* renderer, NGuiWindow* window, glm::vec2 offset);

    glm::vec2 elemPos;
    float pixels;

   public:
    void text(const char* text, ...);
    void inputLine(char* out, size_t len, const char* empty = "");
    void image(glm::vec2 sz, gfx::BaseTexture* texture);
    void progressBar(float value, float max);
    bool button(const char* text);
  };
  glm::vec2 getPosition() { return position; }

  void setTitle(std::string title) { this->title = title; };

  void setMaxSize(glm::vec2 maxSize) { this->maxSize = maxSize; }
  void setMinSize(glm::vec2 minSize) { this->minSize = minSize; }
  void setSize(glm::vec2 size) {
    this->size = glm::min(maxSize, glm::max(minSize, size));
  }

  glm::vec2 getSize() { return size; }

  void open();
  void close();

  virtual void show(Render* renderer) = 0;

  virtual void render(NGuiRenderer* renderer);
};
}  // namespace rdm::gfx::gui
