#pragma once

#include <glm/ext/vector_int2.hpp>
#include <map>
#include <memory>
#include <string>

#include "SDL3_ttf/SDL_ttf.h"
namespace rdm::gfx::gui {
struct Font {
  TTF_Font* font;
  void* fontDataRef;

  ~Font();

  glm::ivec2 getTextSize(const char* text);
};

struct OutFontTexture {
  void* data;
  int w;
  int h;

  OutFontTexture() { data = NULL; }
  ~OutFontTexture();
};

class FontRender {
 public:
  static OutFontTexture render(Font* font, const char* text);
  static OutFontTexture renderWrapped(Font* font, const char* text,
                                      unsigned int width);
};

class FontCache {
  std::map<std::string, Font> fonts;

 public:
  FontCache();

  std::string toFontName(std::string font, int ptsize);

  Font* get(std::string fontName, int ptsize);
};
};  // namespace rdm::gfx::gui
