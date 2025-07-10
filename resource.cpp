#include "resource.hpp"

#include <assimp/anim.h>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <assimp/types.h>
#include <assimp/vector3.h>
#include <sys/types.h>

#include "filesystem.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/camera.hpp"
#include "gfx/engine.hpp"
#include "gfx/imgui/imgui.h"
#include "gfx/mesh.hpp"
#include "gfx/viewport.hpp"
#include "logging.hpp"
#include "settings.hpp"
namespace rdm {
ResourceManager::ResourceManager() {
  missingTexture = load<resource::Texture>(RESOURCE_MISSING_TEXTURE);
  previewViewport = NULL;
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
    delete io.value();
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

static BaseResource* selectedResource = NULL;
static resource::Model::Animator* animator = NULL;

void ResourceManager::imgui(gfx::Engine* engine) {
  const glm::ivec2 imgSize(256, 256);

  if (!previewViewport) {
    gfx::ViewportGfxSettings settings = gfx::ViewportGfxSettings();
    settings.format = gfx::BaseTexture::RGBA8;
    settings.resolution = imgSize;
    previewViewport = new gfx::Viewport(engine, settings);
    gfx::Camera& cam = previewViewport->getCamera();

    cam.setTarget(glm::vec3(0));
    cam.setFar(1000.f);
    cam.setFOV(45.f);
    cam.setUp(glm::vec3(0.f, 0.f, -1.0));
  }

  if (!animator) {
    animator = new resource::Model::Animator();
    animator->initBuffer(engine->getDevice());
  }

  gfx::Camera& cam = previewViewport->getCamera();
  if (selectedResource) {
    if (resource::Model* model =
            dynamic_cast<resource::Model*>(selectedResource)) {
      resource::Model::BoundingBox box = model->getBoundingBox();
      float v = glm::distance(box.min, box.max);
      glm::vec3 p = (box.min + box.max) / 2.f;
      cam.setPosition(p + glm::vec3(v * sinf(engine->getTime()),
                                    v * cosf(engine->getTime()), 2.f));
      cam.setTarget(p);

      void* _ = engine->setViewport(previewViewport);
      engine->getDevice()->clear(0.f, 0.f, 0.f, 0.f);
      engine->getDevice()->clearDepth();
      model->render(engine->getDevice(), animator);
      engine->finishViewport(_);
    }
  }

  ImGui::Begin("ResourceManager");

  for (auto& [id, resource] : resources) {
    if (ImGui::Button(resource->getName().c_str()))
      selectedResource = resource.get();
  }
  if (selectedResource) {
    ImGui::Text("Type %i, DR: %s", selectedResource->getType(),
                selectedResource->getDataReady() ? "true" : "false");
    if (resource::BaseGfxResource* gfxr =
            dynamic_cast<resource::BaseGfxResource*>(selectedResource)) {
      ImGui::Text("R: %s", gfxr->getReady() ? "true" : "false");
      if (resource::Texture* texture =
              dynamic_cast<resource::Texture*>(selectedResource)) {
        ImGui::Image(texture->getTexture()->getImTextureId(), ImVec2(256, 256),
                     ImVec2(0, 1), ImVec2(1, 0));
      }
      if (resource::Model* model =
              dynamic_cast<resource::Model*>(selectedResource)) {
        ImGui::Image(previewViewport->get()->getImTextureId(),
                     ImVec2(imgSize.x, imgSize.y), ImVec2(0, 1), ImVec2(1, 0));
      }
    }

    selectedResource->imguiDebug();
  }
  ImGui::End();
}

void ResourceManager::deleteGfxResources() {
  for (auto& [rscId, rsc] : resources) {
    if (resource::BaseGfxResource* rscg =
            dynamic_cast<resource::BaseGfxResource*>(rsc.get())) {
      rscg->gfxDelete();
    }
  }
}

resource::BaseGfxResource::BaseGfxResource(ResourceManager* manager,
                                           std::string name)
    : BaseResource(manager, name) {
  isReady = false;
}
};  // namespace rdm
