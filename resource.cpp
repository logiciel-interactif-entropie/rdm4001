#include "resource.hpp"

#include <sys/types.h>

#include <stdexcept>

#include "filesystem.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/stb_image.h"
#include "settings.hpp"
namespace rdm {
static resource::Texture* missingTexture;

ResourceManager::ResourceManager() {
  if (!missingTexture)
    missingTexture = dynamic_cast<resource::Texture*>(
        load(BaseResource::Texture, "dat5/missingtexture.png"));
}

void BaseResource::loadData() {
  common::OptionalData data =
      common::FileSystem::singleton()->readFile(name.c_str());
  if (data) {
    onLoadData(data);
    isDataReady = true;
    Log::printf(LOG_DEBUG, "Loaded resource data %s", name.c_str());
  } else {
    Log::printf(LOG_ERROR, "Could not load resource path %s", name.c_str());
  }
  needsData = false;
}

static CVar rsc_load_quota("rsc_load_quota", "100", CVARF_SAVE | CVARF_GLOBAL);

void ResourceManager::tick() {
  int quota_amt = 0;
  for (auto& [rscId, rsc] : resources) {
    if (rsc->getNeedsData()) {
      rsc->loadData();
      quota_amt++;

      if (quota_amt > rsc_load_quota.getInt()) break;
    }
  }
}

BaseResource* ResourceManager::load(BaseResource::Type type,
                                    const char* resourceName) {
  if (BaseResource* rsc = getResource(resourceName)) return rsc;

  if (auto io = common::FileSystem::singleton()->getFileIO(resourceName, "r")) {
    BaseResource* rsc;
    switch (type) {
      case BaseResource::Texture:
        rsc = new resource::Texture(this, resourceName);
        break;
      case BaseResource::Model:
        rsc = new resource::Model(this, resourceName);
        break;
      default:
        throw std::runtime_error("");
    }
    resources[resourceName].reset(rsc);
    return rsc;
  } else
    return NULL;
}

static CVar r_upload_quota("r_upload_quota", "100", CVARF_SAVE | CVARF_GLOBAL);

void ResourceManager::tickGfx(gfx::Engine* engine) {
  int quota_amt = 0;
  for (auto& [rscId, rsc] : resources) {
    if (resource::BaseGfxResource* rscg =
            dynamic_cast<resource::BaseGfxResource*>(rsc.get())) {
      if (!rscg->getReady() && rscg->getDataReady()) {
        Log::printf(LOG_DEBUG, "Loaded gfx resource for %s",
                    rscg->getName().c_str());
        rscg->gfxUpload(engine);
        quota_amt++;

        if (quota_amt > r_upload_quota.getInt()) {
          break;
        }
      }
    }
  }
}

void ResourceManager::deleteGfxResources() {
  for (auto& [rscId, rsc] : resources) {
    if (resource::BaseGfxResource* rscg =
            dynamic_cast<resource::BaseGfxResource*>(rsc.get())) {
      rscg->gfxDelete();
    }
  }
}

namespace resource {
BaseGfxResource::BaseGfxResource(ResourceManager* manager, std::string name)
    : BaseResource(manager, name) {
  isReady = false;
}

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
    if (this == missingTexture)
      return NULL;
    else
      return missingTexture->getTexture();
  }
}

Model::Model(ResourceManager* rm, std::string name)
    : BaseGfxResource(rm, name) {}

void Model::gfxDelete() {}

void Model::gfxUpload(gfx::Engine* engine) { setReady(); }

void Model::onLoadData(common::OptionalData data) {}
};  // namespace resource
};  // namespace rdm
