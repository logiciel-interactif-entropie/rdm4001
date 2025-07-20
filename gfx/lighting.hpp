#pragma once
#include <memory>

#include "base_types.hpp"
namespace rdm::gfx {
class Engine;
class LightingManager {
  std::unique_ptr<BaseBuffer> sunBuffer;

  struct SunUpload {
    glm::vec4 ambient, diffuse, specular, direction;
  };

 public:
  LightingManager(gfx::Engine* engine);

  struct Light {};

  struct Sun {
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    glm::vec3 direction;

    SunUpload convert() {
      SunUpload u;
      u.ambient = glm::vec4(ambient, 1.0);
      u.diffuse = glm::vec4(diffuse, 1.0);
      u.specular = glm::vec4(specular, 1.0);
      u.direction = glm::vec4(direction, 1.0);
      return u;
    }
  };

  Sun getSun() { return sun; }
  void setSun(Sun sun) {
    this->sun = sun;
    dirtySun = true;
  }

  void tick();
  void upload(glm::vec3 origin, gfx::BaseProgram* bp);

 private:
  Sun sun;
  bool dirtySun;
};
}  // namespace rdm::gfx
