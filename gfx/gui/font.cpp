#include "font.hpp"

#include <string.h>

#include <format>

#include "SDL_error.h"
#include "filesystem.hpp"
#include "logging.hpp"
namespace rdm::gfx::gui {
glm::ivec2 Font::getTextSize(const char* text) {
  glm::ivec2 p;
  TTF_SizeUTF8(font, text, &p.x, &p.y);
  return p;
}

OutFontTexture FontRender::render(Font* font, const char* text) {
  OutFontTexture t;

  SDL_Surface* surf;
  SDL_Color color;
  color.r = 255;
  color.g = 255;
  color.b = 255;
  color.a = 255;
  surf = TTF_RenderUTF8_Blended(font->font, text, color);
  if (!surf) {
    Log::printf(LOG_ERROR, "TTF render returned null, %s", SDL_GetError());
    t.data = NULL;
    return t;
  }

  SDL_Surface* conv =
      SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_LockSurface(conv);
  size_t size = conv->w * conv->h * 4;
  t.data = new char[size];
  t.w = conv->w;
  t.h = conv->h;

  // https://stackoverflow.com/questions/65815332/flipping-a-surface-vertically-in-sdl2
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
  SDL_FreeSurface(conv);
  SDL_FreeSurface(surf);

  return t;
}

OutFontTexture FontRender::renderWrapped(Font* font, const char* text,
                                         unsigned int wraplength) {
  OutFontTexture t;

  SDL_Surface* surf;
  SDL_Color color;
  color.r = 255;
  color.g = 255;
  color.b = 255;
  color.a = 255;
  surf = TTF_RenderUTF8_Blended_Wrapped(font->font, text, color, wraplength);
  if (!surf) {
    Log::printf(LOG_ERROR, "TTF render returned null, %s", SDL_GetError());
    t.data = NULL;
    return t;
  }

  SDL_Surface* conv =
      SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_LockSurface(conv);
  size_t size = conv->w * conv->h * 4;
  t.data = new char[size];
  t.w = conv->w;
  t.h = conv->h;

  // https://stackoverflow.com/questions/65815332/flipping-a-surface-vertically-in-sdl2
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
  SDL_FreeSurface(conv);
  SDL_FreeSurface(surf);

  return t;
}

OutFontTexture::~OutFontTexture() {
  if (data) delete data;
}

Font::~Font() {}

FontCache::FontCache() {
  if (int error = TTF_Init() != 0) {
    Log::printf(LOG_FATAL, "TTF_Init != 0 (%i)", error);
  }
}

std::string FontCache::toFontName(std::string font, int ptsize) {
  return std::format("{}-{}", font, ptsize);
}

Font* FontCache::get(std::string fontName, int ptsize) {
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
      SDL_RWops* fontMem = SDL_RWFromConstMem(dcpy, ds->size());
      TTF_Font* fontOut = TTF_OpenFontRW(fontMem, 0, ptsize);
      if (!fontOut) {
        throw std::runtime_error("Error creating font");
      }
      Font f;
      f.font = fontOut;
      fonts[_fontName] = f;
      return &fonts[_fontName];
    } else {
      return NULL;
    }
  }
}
}  // namespace rdm::gfx::gui
