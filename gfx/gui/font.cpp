#include "font.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string.h>

#include <format>

#include "filesystem.hpp"
#include "logging.hpp"
namespace rdm::gfx::gui {
glm::ivec2 Font::getTextSize(const char* text) {
  glm::ivec2 p;
  // TTF_GetStringSize(font, text, strlen(text), &p.x, &p.y);
  return p;
}

OutFontTexture FontRender::render(Font* font, const char* text) {
  /*  OutFontTexture t;

  SDL_Surface* surf;
  SDL_Color color;
  color.r = 255;
  color.g = 255;
  color.b = 255;
  color.a = 255;
  surf = TTF_RenderText_Blended(font->font, text, strlen(text), color);
  if (!surf) {
    Log::printf(LOG_ERROR, "TTF render returned null");
    t.data = NULL;
    return t;
  }

  SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
  SDL_LockSurface(conv);
  size_t size = conv->w * conv->h * 4;
  t.data = new char[size];
  t.w = conv->w;
  t.h = conv->h;

  //
  https://stackoverflow.com/questions/65815332/flipping-a-surface-vertically-in-sdl2
  int pitch = conv->pitch;
  char* temp = new char[pitch];
  char* pixels = (char*)conv->pixels;
  for (int i = 0; i < conv->h / 2; ++i) {
    // get pointers to the two rows to swap
    char* row1 = pixels + i * pitch;
    char* row2 = pixels + (surf->h - i - 1) * pitch;

    // swap rows
    memcpy(temp, row1, pitch);
    memcpy(row1, row2, pitch);
    memcpy(row2, temp, pitch);
  }

  memcpy(t.data, conv->pixels, size);
  SDL_UnlockSurface(conv);
  SDL_DestroySurface(conv);
  SDL_DestroySurface(surf);*/

  unsigned int t;
  t = 0xffffffff;
  OutFontTexture otf;
  otf.data = &t;
  otf.w = 1;
  otf.h = 1;

  return otf;
}

OutFontTexture FontRender::renderWrapped(Font* font, const char* text,
                                         unsigned int wraplength) {
  unsigned int t;
  t = 0xffffffff;
  OutFontTexture otf;
  otf.data = &t;
  otf.w = 1;
  otf.h = 1;

  return otf;
}

OutFontTexture::~OutFontTexture() {
  // if (data) free(data);
}

Font::~Font() {
  // TTF_CloseFont(font);
  free(fontDataRef);
}

FontCache::FontCache() {
  /*
  if (int error = TTF_Init() == 0) {
    Log::printf(LOG_FATAL, "TTF_Init != 0 (%i)", error);
  }
  */
}

std::string FontCache::toFontName(std::string font, int ptsize) {
  return std::format("{}-{}", font, ptsize);
}

Font* FontCache::get(std::string fontName, int ptsize) {
  /*
  std::string _fontName = toFontName(fontName, ptsize);
  auto it = fonts.find(_fontName);
  if (it != fonts.end()) {
    return &fonts[_fontName];
  } else {
    common::OptionalData ds =
        common::FileSystem::singleton()->readFile(fontName.c_str());
    if (ds) {
      void* dcpy = malloc(ds->size());
      memcpy(dcpy, ds->data(), ds->size());
      SDL_IOStream* fontMem = SDL_IOFromConstMem(dcpy, ds->size());
      TTF_Font* fontOut = TTF_OpenFontIO(fontMem, true, ptsize);
      if (!fontOut) {
        // rdm::Log::printf(rdm::LOG_ERROR, "fontOut = NULL, %s",
        // SDL_GetError());
        throw std::runtime_error("Error creating font");
      }
      Font& f = fonts[_fontName];
      f.font = fontOut;
      f.fontDataRef = dcpy;
      return &f;
    } else {
      return NULL;
    }
  }
  */
  return NULL;
}
}  // namespace rdm::gfx::gui
