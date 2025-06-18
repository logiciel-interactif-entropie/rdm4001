#pragma once
#include <assimp/scene.h>

#include <functional>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "filesystem.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/material.hpp"
#include "gfx/mesh.hpp"
#include "gfx/viewport.hpp"
namespace rdm {
namespace gfx {
class Engine;
};

class ResourceManager;
namespace resource {
class Texture;
}

class BaseResource {
  std::string name;
  bool isDataReady;
  bool needsData;
  ResourceManager* resourceManager;

 public:
  BaseResource(ResourceManager* manager, std::string name) {
    this->name = name;
    needsData = true;
    isDataReady = false;
    resourceManager = manager;
  }
  virtual ~BaseResource() = default;

  std::string getName() { return name; }

  enum Type {
    Texture,
    Model,
  };

  virtual Type getType() = 0;
  virtual void onLoadData(common::OptionalData data) = 0;

  bool getDataReady() { return isDataReady; }
  void setDataReady() { isDataReady = true; }
  bool getNeedsData() { return needsData; }
  void setNeedsData() { needsData = true; }

  void loadData();

  ResourceManager* getResourceManager() { return resourceManager; };

  virtual void imguiDebug() {};

  std::mutex m;
};

typedef std::string ResourceId;

class ResourceManager {
  std::unordered_map<ResourceId, std::unique_ptr<BaseResource>> resources;
  resource::Texture* missingTexture;
  gfx::Viewport* previewViewport;

 public:
  ResourceManager();
  resource::Texture* getMissingTexture() { return missingTexture; };

  BaseResource* getResource(ResourceId id) {
    if (resources.find(id) != resources.end())
      return resources[id].get();
    else
      return NULL;
  }

  BaseResource* load(BaseResource::Type type, const char* resourceName);
  template <typename T>
  T* load(const char* resourceName) {
    if (BaseResource* rsc = getResource(resourceName)) {
      if (T* crsc = dynamic_cast<T*>(rsc)) {
        return crsc;
      } else {
        return NULL;
      }
    }

    if (auto io =
            common::FileSystem::singleton()->getFileIO(resourceName, "r")) {
      T* rsc = new T(this, resourceName);
      resources[resourceName].reset(rsc);
      return rsc;
    } else
      return NULL;
  }

  void tick();
  void tickGfx(gfx::Engine* engine);
  void imgui(gfx::Engine* engine);

  void deleteGfxResources();
};

namespace resource {
class BaseGfxResource : public BaseResource {
  bool isReady;

 public:
  BaseGfxResource(ResourceManager* manager, std::string name);

  virtual void gfxDelete() = 0;
  virtual void gfxUpload(gfx::Engine* engine) = 0;

  bool getReady() { return isReady; }
  void setReady() { isReady = true; }
};

class Texture : public BaseGfxResource {
  std::unique_ptr<gfx::BaseTexture> texture;
  void* textureData;

  int width;
  int height;
  int channels;

 public:
  Texture(ResourceManager* rm, std::string name);

  gfx::TextureCache::Info getInfo();

  virtual void gfxDelete();
  virtual void gfxUpload(gfx::Engine* engine);

  virtual void onLoadData(common::OptionalData data);

  virtual Type getType() { return BaseResource::Texture; }
  gfx::BaseTexture* getTexture();
};

#define MODEL_MAX_BONE_TRANSFORMS 128

class Model : public BaseGfxResource {
  Assimp::Importer importer;

  const aiScene* scene;

  struct Texture {
    bool external;
    int textureId;
    std::unique_ptr<gfx::BaseTexture> texture;
    resource::Texture* texture_ref;
  };

  struct Material {
    Texture diffuse;
  };

  std::map<std::string, gfx::Mesh> meshes;
  std::map<std::string, gfx::BoneInfo> boneInfo;
  std::shared_ptr<gfx::Material> gfx_material;
  std::map<std::string, Material> materials;
  std::vector<Texture*> deferedTextures;

  glm::mat4 inverseGlobalTransform;

  std::string path;

  int boneCount;
  bool broken;
  bool skinned;

 public:
  Model(ResourceManager* rm, std::string name);

  virtual void gfxDelete();
  virtual void gfxUpload(gfx::Engine* engine);

  virtual void onLoadData(common::OptionalData data);
  virtual Type getType() { return BaseResource::Model; }

  struct KeyTranslate {
    glm::vec3 position;
    double timestamp;
  };

  struct KeyRotate {
    glm::quat quat;
    double timestamp;
  };

  struct KeyScale {
    glm::vec3 scale;
    double timestamp;
  };

  struct BoneKeyframe {
    std::vector<KeyTranslate> translations;
    std::vector<KeyRotate> rotations;
    std::vector<KeyScale> scales;
  };

  struct Animation {
    std::unordered_map<std::string, BoneKeyframe> boneKeys;

    double tps;
    double duration;
  };

  struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;
  };

 private:  // UGLYUGLYUGLYUGLY
  Animation* preferedAnimation;
  BoundingBox boundingBox;

 public:
  struct Animator {
    Animation* animation;
    double currentTime;
    double speed;
    glm::mat4 boneMatrices[MODEL_MAX_BONE_TRANSFORMS];

    Animator() { reset(); }

    void reset() {
      animation = NULL;
      speed = 1.f;
      currentTime = 0.f;
      for (int i = 0; i < MODEL_MAX_BONE_TRANSFORMS; i++)
        boneMatrices[i] = glm::mat4(1.f);
    }

    void upload(gfx::BaseProgram* program);
  };

  void render(
      gfx::BaseDevice* device, Animator* animator = NULL,
      gfx::Material* material = NULL,
      std::optional<std::function<void(gfx::BaseProgram*)>> setParameters = {});

  void updateAnimator(gfx::Engine* engine, Animator* anim);
  glm::mat4 getBoneTransform(std::string name, Animator* anim);
  Animation* getAnimation(std::string name);
  Animation* getAnimation() { return preferedAnimation; }

  virtual void imguiDebug();
  BoundingBox getBoundingBox() { return boundingBox; }

 private:
  std::map<std::string, Animation> animations;

  void calcAnimatorTransforms(aiNode* node, Animator* anim,
                              glm::mat4 transform);

  int positionIdx(BoneKeyframe k, float t) const;
  int rotationIdx(BoneKeyframe k, float t) const;
  int scaleIdx(BoneKeyframe k, float t) const;

  glm::mat4 interpPosition(BoneKeyframe k, float t) const;
  glm::mat4 interpRotation(BoneKeyframe k, float t) const;
  glm::mat4 interpScale(BoneKeyframe k, float t) const;

  glm::mat4 boneTransform(BoneKeyframe k, float t) const;
};

};  // namespace resource
}  // namespace rdm
