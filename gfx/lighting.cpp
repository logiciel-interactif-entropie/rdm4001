#include "lighting.hpp"

#include <glm/geometric.hpp>

#include "base_types.hpp"
#include "engine.hpp"
namespace rdm::gfx {
Light::Light(LightingManager* manager) {
  this->manager = manager;
  dirty = true;
}

Light::~Light() { manager->deleteLight(this); }

LightingManager::LightingManager(gfx::Engine* engine) {
  this->engine = engine;
  dc = false;

  sunBuffer = engine->getDevice()->createBuffer();
  sun.direction = glm::vec3(0.5, 0.5, 0.5);
  sun.ambient = glm::vec3(0.05, 0.05, 0.05);
  sun.specular = glm::vec3(1.0);
  sun.diffuse = glm::vec3(0.6, 0.6, 0.6);
  SunUpload up = sun.convert();
  sunBuffer->upload(BaseBuffer::Uniform, BaseBuffer::StaticRead,
                    sizeof(SunUpload), &up);
  dirtySun = false;

  createLight();
}

void LightingManager::tick() {
  if (dirtySun) {
    SunUpload up = sun.convert();
    sunBuffer->uploadSub(0, sizeof(SunUpload), &up);
    dirtySun = false;
  }
}

void LightingManager::upload(glm::vec3 origin, gfx::BaseProgram* bp) {
  std::vector<Light*> lights;
  for (auto& light : this->lights) {
    if (lights.size() == NR_MAX_LIGHTS) break;
    if (glm::distance(light->position, origin) > 1000.f) break;
    lights.push_back(light.get());
  }

  int idx = 0;
  for (auto& light : lights) {
    /*bp->setParameter(
        std::format("lights[{}]", idx), DtBuffer,
        {.buffer.buffer = light->data.get(), .buffer.slot = 11 + idx});*/
    // idx++;
  }
  bp->setParameter("lightCount", DtInt, {.integer = idx});

  bp->setParameter("SunBlock", gfx::DtBuffer,
                   {.buffer = {.slot = 10, .buffer = sunBuffer.get()}});
}

LightingManager::~LightingManager() {
  dc = true;
  lights.clear();
}

void LightingManager::deleteLight(Light* light) {
  if (dc) return;

  std::vector<std::unique_ptr<Light>>::iterator it;
  bool found;
  for (auto& _light : lights) {
    if (_light.get() == light) {
      it = std::find(lights.begin(), lights.end(), _light);
      found = true;
      break;
    }
  }
  if (found) lights.erase(it);
}

Light* LightingManager::createLight() {
  Light* l = new Light(this);
  lights.push_back(std::unique_ptr<Light>(l));
  return l;
}
};  // namespace rdm::gfx
