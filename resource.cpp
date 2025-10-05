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
#include <lua.h>
#include <sys/types.h>

#include <stdexcept>

#include "filesystem.hpp"
#include "game.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/camera.hpp"
#include "gfx/engine.hpp"
#include "gfx/imgui/imgui.h"
#include "gfx/lighting.hpp"
#include "gfx/mesh.hpp"
#include "gfx/viewport.hpp"
#include "logging.hpp"
#include "object.hpp"
#include "object_property.hpp"
#include "script/script_api.hpp"
#include "settings.hpp"
#include "worker.hpp"
namespace rdm {
RDM_REFLECTION_BEGIN_DESCRIBED(ResourceManager);
RDM_REFLECTION_PROPERTY_FUNCTION(
    ResourceManager, LoadTexture, [](lua_State* L) {
      ResourceManager* rmgr =
          script::ObjectBridge::getDescribed<ResourceManager>(L, 1);
      resource::Texture* texture =
          rmgr->load<resource::Texture>(lua_tostring(L, 2));
      script::ObjectBridge::pushDescribed(L, texture);
      return 1;
    });
RDM_REFLECTION_PROPERTY_FUNCTION(ResourceManager, LoadModel, [](lua_State* L) {
  ResourceManager* rmgr =
      script::ObjectBridge::getDescribed<ResourceManager>(L, 1);
  resource::Model* model = rmgr->load<resource::Model>(lua_tostring(L, 2));
  script::ObjectBridge::pushDescribed(L, model);
  return 1;
});
RDM_REFLECTION_END_DESCRIBED();

RDM_REFLECTION_BEGIN_DESCRIBED(BaseResource);
RDM_REFLECTION_PROPERTY_STRING(BaseResource, Name, &BaseResource::getName,
                               NULL);
RDM_REFLECTION_END_DESCRIBED();

namespace resource {
RDM_REFLECTION_BEGIN_DESCRIBED(BaseGfxResource);
}

ResourceManager::ResourceManager() {
  missingTexture = load<resource::Texture>(RESOURCE_MISSING_TEXTURE);
  missingModel = load<resource::Model>(RESOURCE_MISSING_MODEL);

  resource::Texture::TextureSettings ts;
  ts.minFiltering = gfx::BaseTexture::Nearest;
  ts.maxFiltering = gfx::BaseTexture::Nearest;
  missingTexture->setTextureSettings(ts);
  previewViewport = NULL;
}

void BaseResource::loadData() {
  if (broken) return;

  common::OptionalData data =
      common::FileSystem::singleton()->readFile(getName().c_str());
  if (data) {
    onLoadData(data);
    isDataReady = true;
    Log::printf(LOG_DEBUG, "Loaded resource data %s", name.c_str());
  } else {
    Log::printf(LOG_ERROR, "Could not load resource path %s", name.c_str());
    broken = true;
  }
  needsData = false;
}

static CVar rsc_load_quota("rsc_load_quota", "5", CVARF_SAVE | CVARF_GLOBAL);

void ResourceManager::startTaskForResource(BaseResource* br) {
  br->setNeedsData(false);
  WorkerManager::singleton()->run([br] { br->loadData(); });
}

void ResourceManager::tick() {
  int quota_amt = 0;
  for (auto& [rscId, rsc] : resources) {
    if (rsc->getNeedsData()) {
      quota_amt++;

      if (quota_amt > rsc_load_quota.getInt()) break;
    }
  }
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

    gfx::LightingManager& lm = previewViewport->getLightingManager();
    gfx::LightingManager::Sun sun = lm.getSun();
    sun.ambient = glm::vec3(0.0);
    sun.diffuse = glm::vec3(1.0);
    sun.specular = glm::vec3(1.0);
    lm.setSun(sun);
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

  int c = 0;
  for (auto& [id, resource] : resources) {
    char p[64];
    snprintf(p, 64, "%016lx", id);
    if (ImGui::Button(p)) {
      selectedResource = resource.get();
    }
    c++;
    if (c == 4) {
      ImGui::Separator();
      c = 0;
    } else
      ImGui::SameLine();
  }
  ImGui::Separator();
  if (selectedResource) {
    ImGui::Text("Type %i, DR: %s", selectedResource->getType(),
                selectedResource->getDataReady() ? "true" : "false");
    ImGui::Text("%s", selectedResource->getName().c_str());
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
