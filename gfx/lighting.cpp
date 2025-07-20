#include "lighting.hpp"

#include "base_types.hpp"
#include "engine.hpp"
namespace rdm::gfx {
LightingManager::LightingManager(gfx::Engine* engine) {
  sunBuffer = engine->getDevice()->createBuffer();
  sun.direction = glm::vec3(0.5, 0.5, 0.5);
  sun.ambient = glm::vec3(0.1, 0.1, 0.1);
  sun.diffuse = glm::vec3(0.7, 0.7, 0.7);
  SunUpload up = sun.convert();
  sunBuffer->upload(BaseBuffer::Uniform, BaseBuffer::StaticRead,
                    sizeof(SunUpload), &up);
  dirtySun = false;
}

void LightingManager::tick() {
  if (dirtySun) {
    SunUpload up = sun.convert();
    sunBuffer->uploadSub(0, sizeof(SunUpload), &up);
    dirtySun = false;
  }
}

void LightingManager::upload(glm::vec3 origin, gfx::BaseProgram* bp) {
  bp->setParameter("SunBlock", gfx::DtBuffer,
                   {.buffer = {.slot = 10, .buffer = sunBuffer.get()}});
}
};  // namespace rdm::gfx
