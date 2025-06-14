#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/stb_image.h"
#include "resource.hpp"

namespace rdm::resource {
Texture::Texture(ResourceManager* manager, std::string name)
    : BaseGfxResource(manager, name) {}

void Texture::gfxDelete() {
  std::scoped_lock l(m);
  texture.reset();
}

void Texture::gfxUpload(gfx::Engine* engine) {
  std::scoped_lock l(m);

  gfx::BaseTexture::Format fmt;
  switch (channels) {
    case 3:
      fmt = gfx::BaseTexture::RGB;
      break;
    case 4:
      fmt = gfx::BaseTexture::RGBA;
      break;
    default:
      return;
  }
  texture = engine->getDevice()->createTexture();
  texture->upload2d(width, height, gfx::DtUnsignedByte, fmt, textureData, 4);
  setReady();
}

gfx::TextureCache::Info Texture::getInfo() {
  gfx::TextureCache::Info info;
  info.channels = channels;
  info.width = width;
  info.height = height;
  info.data = NULL;
  switch (info.channels) {
    case 3:
      info.format = gfx::BaseTexture::RGB;
      break;
    case 4:
      info.format = gfx::BaseTexture::RGBA;
      break;
    default:
      break;
  }
  return info;
}

void Texture::onLoadData(common::OptionalData data) {
  std::scoped_lock l(m);

  stbi_set_flip_vertically_on_load(true);
  stbi_uc* uc = stbi_load_from_memory(data->data(), data->size(), &width,
                                      &height, &channels, 0);
  textureData = (void*)uc;
}

gfx::BaseTexture* Texture::getTexture() {
  std::scoped_lock l(m);

  if (texture) {
    m.unlock();
    return texture.get();
  } else {
    if (!getDataReady()) {
      setNeedsData();
    }
    if (this == getResourceManager()->getMissingTexture())
      return NULL;
    else
      return getResourceManager()->getMissingTexture()->getTexture();
  }
}
};  // namespace rdm::resource
