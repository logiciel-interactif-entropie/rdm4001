#pragma once
#include <memory>

#include "base_types.hpp"
namespace rdm::gfx {
// keep synced with deferred_post.fs.glsl
#define NR_MAX_LIGHTS 16

class Engine;
class LightingManager;
class Light {
  friend class LightingManager;
  LightingManager* manager;
  glm::vec3 position;
  bool dirty;
  Light(LightingManager* manager);

 public:
  enum Type {
    Point,
    Spot,
  };

  ~Light();

  glm::vec3 getPosition() { return position; }
  void setPosition(glm::vec3 p) {
    position = p;
    dirty = true;
  }

  void setType(Type t) {
    type = t;
    dirty = true;
  }
  Type getType() { return type; }

 private:
  Type type;
};

class LightingManager {
  friend class Light;

  bool dc;
  gfx::Engine* engine;

  std::unique_ptr<BaseBuffer> sunBuffer;
  std::vector<std::unique_ptr<Light>> lights;

  struct SunUpload {
    glm::vec4 ambient, diffuse, specular, direction;
  };

  struct LightsUpload {
    struct {
      glm::vec4 position, diffuse, specular;
    } light[NR_MAX_LIGHTS];
  };

  void deleteLight(Light* light);

 public:
  LightingManager(gfx::Engine* engine);
  ~LightingManager();

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

  Light* createLight();

  gfx::Engine* getEngine() { return engine; }

 private:
  Sun sun;
  bool dirtySun;
};
}  // namespace rdm::gfx
