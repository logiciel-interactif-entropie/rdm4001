#pragma once
#include <assimp/scene.h>

#include <functional>
#include <glm/ext/vector_float4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#include "filesystem.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/material.hpp"
#include "gfx/mesh.hpp"
#include "gfx/rendercommand.hpp"
#include "gfx/viewport.hpp"
#include "object.hpp"
namespace rdm {
namespace gfx {
class Engine;
};

class ResourceManager;
namespace resource {
class Texture;
class Model;
}  // namespace resource

#define RESOURCE_MISSING_TEXTURE "engine/assets/missingtexture.png"
#define RESOURCE_MISSING_MODEL "engine/assets/error.glb"

class BaseResource : public reflection::Object {
  RDM_OBJECT;
  RDM_OBJECT_DEF(BaseResource, reflection::Object);

  std::string name;
  bool isDataReady;
  bool needsData;
  bool broken;
  ResourceManager* resourceManager;

 public:
  BaseResource(ResourceManager* manager, std::string name) {
    this->name = name;
    needsData = true;
    isDataReady = false;
    resourceManager = manager;
    broken = false;
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
  void setNeedsData(bool v = true) { needsData = v; }

  void loadData();

  ResourceManager* getResourceManager() { return resourceManager; };

  virtual void imguiDebug() {};

  std::mutex m;
};

typedef uint64_t ResourceId;

class ResourceManager : public reflection::Object {
  RDM_OBJECT;
  RDM_OBJECT_DEF(ResourceManager, reflection::Object);

  std::unordered_map<ResourceId, std::unique_ptr<BaseResource>> resources;
  resource::Texture* missingTexture;
  resource::Model* missingModel;
  gfx::Viewport* previewViewport;

  void startTaskForResource(BaseResource* br);

 public:
  ResourceManager();
  resource::Texture* getMissingTexture() { return missingTexture; };

  BaseResource* getResource(ResourceId id) {
    if (resources.find(id) != resources.end())
      return resources[id].get();
    else
      return NULL;
  }

  template <typename T>
  T* load(const char* resourceName) {
    ResourceId id = hash(resourceName);

    if (BaseResource* rsc = getResource(id)) {
      if (T* crsc = dynamic_cast<T*>(rsc)) {
        return crsc;
      } else {
        throw std::runtime_error("ResourceManager::load, invalid type");
      }
    }

    T* rsc = new T(this, resourceName);
    resources[id].reset(rsc);
    startTaskForResource(rsc);
    return rsc;
  }

  bool getResourceAvailable(const char* resourceName) {
    return (common::FileSystem::singleton()
                ->getFileIO(resourceName, "r")
                .has_value());
  }

  void tick();
  void tickGfx(gfx::Engine* engine);
  void imgui(gfx::Engine* engine);

  void deleteGfxResources();

  static constexpr ResourceId hash(const char* input) {
    return std::hash<std::string>{}(input);
  }
};

#define RID(n) (rdm::ResourceManager::hash(n))

namespace resource {
class BaseGfxResource : public BaseResource {
  RDM_OBJECT;
  RDM_OBJECT_DEF(BaseGfxResource, BaseResource);

  bool isReady;

 public:
  BaseGfxResource(ResourceManager* manager, std::string name);

  virtual void gfxDelete() = 0;
  virtual void gfxUpload(gfx::Engine* engine) = 0;

  bool getReady() { return isReady; }
  void setReady() { isReady = true; }
};

class Texture : public BaseGfxResource {
  RDM_OBJECT;
  RDM_OBJECT_DEF(Texture, BaseGfxResource);

  std::unique_ptr<gfx::BaseTexture> texture;
  enum TextureHandler {
    Unloaded,
    Ktx2,
    Stbi,
  };

  TextureHandler handler;

  void* textureData;

  int width;
  int height;
  int channels;

  bool dirtyTextureSettings;

 public:
  Texture(ResourceManager* rm, std::string name);
  virtual ~Texture();

  gfx::TextureCache::Info getInfo();

  virtual void gfxDelete();
  virtual void gfxUpload(gfx::Engine* engine);

  int getWidth() { return width; };
  int getHeight() { return height; };

  virtual void onLoadData(common::OptionalData data);

  virtual Type getType() { return BaseResource::Texture; }
  gfx::BaseTexture* getTexture();

  struct TextureSettings {
    gfx::BaseTexture::Filtering minFiltering;
    gfx::BaseTexture::Filtering maxFiltering;
  };

  TextureSettings getTextureSettings() { return textureSettings; }
  void setTextureSettings(TextureSettings ts) {
    textureSettings = ts;
    dirtyTextureSettings = true;
  }

 private:
  TextureSettings textureSettings;
};

#define MODEL_MAX_BONE_TRANSFORMS 128

class Model : public BaseGfxResource {
  RDM_OBJECT;
  RDM_OBJECT_DEF(Model, BaseGfxResource);

  Assimp::Importer importer;

  const aiScene* scene;

  struct Texture {
    bool external;
    int textureId;
    std::unique_ptr<gfx::BaseTexture> texture;
    resource::Texture* texture_ref;
  };

  struct Material {
    bool hasAlbedo;
    Texture diffuse;
    glm::vec3 albedo;

    float roughness;
    float metallic;
    float specular;
    float rimLight;

    struct Upload {
      glm::vec4 albedo;
      glm::vec4 pbrMaterials;

      Upload(Material& material) {
        this->albedo = glm::vec4(material.albedo, 1.0);
        pbrMaterials.x = material.roughness;
        pbrMaterials.y = material.metallic;
        pbrMaterials.z = material.specular;
        pbrMaterials.w = material.rimLight;
      }
    };

    std::unique_ptr<gfx::BaseBuffer> pbrData;
  };

  std::map<std::string, gfx::Mesh> meshes;
  std::map<std::string, gfx::BoneInfo> boneInfo;
  std::shared_ptr<gfx::Material> gfx_material;
  std::shared_ptr<gfx::Material> gfx_materialDf;
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
    std::unique_ptr<gfx::BaseBuffer> boneUniformBuffer;
    glm::mat4 boneMatrices[MODEL_MAX_BONE_TRANSFORMS];

    Animator() { reset(); }

    void initBuffer(gfx::BaseDevice* device) {
      if (boneUniformBuffer) return;
      boneUniformBuffer = device->createBuffer();
      boneUniformBuffer->upload(gfx::BaseBuffer::Uniform,
                                gfx::BaseBuffer::DynamicDraw,
                                sizeof(boneMatrices), boneMatrices);
    }

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
