#pragma once

#include <SDL2/SDL_ttf.h>

#include <map>
#include <memory>
#include <string>
namespace rdm::gfx::gui {
struct Font {
  TTF_Font* font;

  ~Font();
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
  static OutFontTexture renderWrapped(Font* font, const char* text, int width);
};

class FontCache {
  std::map<std::string, Font> fonts;

 public:
  FontCache();

  std::string toFontName(std::string font, int ptsize);

  Font* get(std::string fontName, int ptsize);
};
};  // namespace rdm::gfx::gui
