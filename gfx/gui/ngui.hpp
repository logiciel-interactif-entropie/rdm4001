#pragma once
#include <functional>
#include <map>
#include <string>

#include "font.hpp"
#include "gfx/base_types.hpp"
#include "gfx/material.hpp"
#include "gfx/rendercommand.hpp"
#include "gfx/renderpass.hpp"

namespace rdm {
class Game;
class ResourceManager;
namespace gfx {
class Engine;
};
};  // namespace rdm

namespace rdm::gfx::gui {
class NGui;
class NGuiManager;
class NGuiRenderer;

typedef std::function<NGui*(NGuiManager* gui, gfx::Engine* engine)> GUICtor;
class NGuiSingleton {
  NGuiSingleton();

 public:
  static NGuiSingleton* singleton();

  std::map<std::string, GUICtor> guiCtor;
};

class NGui {
  NGuiManager* gui;
  gfx::Engine* engine;

 public:
  NGui(NGuiManager* gui, gfx::Engine* engine);
  virtual ~NGui() {};

  Game* getGame();
  gfx::Engine* getEngine();
  ResourceManager* getResourceManager();
  NGuiManager* getManager() { return gui; }

  virtual void render(NGuiRenderer* renderer) = 0;
};

class NGuiManager {
  std::map<std::string, NGui*> guis;
  gfx::Engine* engine;
  struct CacheTextMember {
    std::string text;
    Font* font;
    std::unique_ptr<gfx::BaseTexture> texture;

    int height, width, maxWidth;
  };
  std::map<int, CacheTextMember> cacheMember;

  std::unique_ptr<BaseBuffer> squareArrayBuffer;
  std::unique_ptr<BaseBuffer> squareElementBuffer;
  std::unique_ptr<BaseArrayPointers> squareArrayPointers;
  std::shared_ptr<gfx::Material> image;
  std::unique_ptr<FontCache> fontCache;

  glm::mat4 uiMatrix;

  char* textInput;
  size_t textInputBufSize;

 public:
  NGuiManager(gfx::Engine* engine);
  void render();

  template <typename T>
  T* getGui() {
    for (auto& [name, gui] : guis) {
      if (T* t = dynamic_cast<T*>(gui)) return t;
    }
    throw std::runtime_error("Cast not found!!!");
  }
  BaseBuffer* getSArrayBuf() { return squareArrayBuffer.get(); }
  BaseBuffer* getSElementBuf() { return squareElementBuffer.get(); }
  BaseArrayPointers* getSArrayPointers() { return squareArrayPointers.get(); }
  FontCache* getFontCache() { return fontCache.get(); }
  gfx::Material* getImageMaterial() { return image.get(); }
  glm::mat4 getUiMatrix() { return uiMatrix; }

  void setCurrentText(char* t, size_t l);
  bool isCurrentText(char* t) { return t == textInput; }

  struct TexOutData {
    gfx::BaseTexture* texture;
    int height;
    int width;
  };
  TexOutData getTextTexture(int tn, Font* font, int maxWidth, const char* text);
};

class NGuiRenderer {
  friend class NGuiManager;
  gfx::Engine* engine;
  NGuiManager* manager;
  NGuiRenderer(NGuiManager* manager, gfx::Engine* engine, int texNum);
  int texNum;
  size_t submittedCommands;

  RenderList list;
  int zIndex;
  RenderCommand* lastCommand;

  glm::vec3 color;

 public:
  std::pair<int, int> text(glm::ivec2 position, Font* font, int maxWidth,
                           const char* text, ...);
  void image(gfx::BaseTexture* image, glm::vec2 position, glm::vec2 size);

  // -1 is not hovering,
  // 0 is hovering,
  // 1 is click
  int mouseDownZone(glm::vec2 position, glm::vec2 size);
  void setZIndex(int index) { zIndex = index; }
  int getZIndex() { return zIndex; }

  void setColor(glm::vec3 color) { this->color = color; };
  RenderCommand* getLastCommand() { return lastCommand; };

  gfx::Engine* getEngine() { return engine; }

  RenderList& getList() { return list; }
};

template <typename T>
class NGuiInstantiator {
 public:
  NGuiInstantiator(const char* nam) {
    NGuiSingleton::singleton()->guiCtor[nam] = [](NGuiManager* gui,
                                                  gfx::Engine* engine) {
      return new T(gui, engine);
    };
  }
};

#define NGUI_INSTANTIATOR(T) \
  static rdm::gfx::gui::NGuiInstantiator<T> __NGUI__##T(#T);
}  // namespace rdm::gfx::gui
