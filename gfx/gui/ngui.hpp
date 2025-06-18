#pragma once
#include <functional>
#include <map>
#include <string>

#include "font.hpp"
#include "gfx/base_types.hpp"
#include "gfx/material.hpp"
#include "gfx/rendercommand.hpp"
#include "gfx/renderpass.hpp"

namespace rdm::gfx {
class Engine;
};

namespace rdm::gfx::gui {
class NGui;
class NGuiManager;

typedef std::function<NGui*(NGuiManager* gui, gfx::Engine* engine)> GUICtor;
class NGuiSingleton {
  NGuiSingleton();

 public:
  static NGuiSingleton* singleton();

  std::map<std::string, GUICtor> guiCtor;
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

 public:
  NGuiManager(gfx::Engine* engine);
  void render();

  BaseBuffer* getSArrayBuf() { return squareArrayBuffer.get(); }
  BaseBuffer* getSElementBuf() { return squareElementBuffer.get(); }
  BaseArrayPointers* getSArrayPointers() { return squareArrayPointers.get(); }
  FontCache* getFontCache() { return fontCache.get(); }
  gfx::Material* getImageMaterial() { return image.get(); }
  glm::mat4 getUiMatrix() { return uiMatrix; }

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
  NGuiRenderer(NGuiManager* manager, gfx::Engine* engine);
  int texNum;

  RenderList list;

  glm::vec3 color;

 public:
  std::pair<int, int> text(glm::ivec2 position, Font* font, int maxWidth,
                           const char* text, ...);

  void setColor(glm::vec3 color) { this->color = color; };

  gfx::Engine* getEngine() { return engine; }
  RenderList& getList() { return list; }
};

class NGui {
  NGuiManager* gui;
  gfx::Engine* engine;

 public:
  NGui(NGuiManager* gui, gfx::Engine* engine);
  virtual ~NGui() {};

  virtual void render(NGuiRenderer* renderer) = 0;
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

#define NGUI_INSTANTIATOR(T) static NGuiInstantiator<T> __NGUI__##T(#T);
}  // namespace rdm::gfx::gui
