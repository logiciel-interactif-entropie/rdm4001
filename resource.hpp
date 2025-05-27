#pragma once
#include <memory>
#include <mutex>
#include <unordered_map>

#include "filesystem.hpp"
#include "gfx/base_types.hpp"
#include "gfx/mesh.hpp"
namespace rdm {
namespace gfx {
class Engine;
};

class ResourceManager;

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

  std::mutex m;
};

typedef std::string ResourceId;

class ResourceManager {
  std::unordered_map<ResourceId, std::unique_ptr<BaseResource>> resources;

 public:
  ResourceManager();

  BaseResource* getResource(ResourceId id) {
    if (resources.find(id) != resources.end())
      return resources[id].get();
    else
      return NULL;
  }

  BaseResource* load(BaseResource::Type type, const char* resourceName);

  void tick();
  void tickGfx(gfx::Engine* engine);

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

  virtual void gfxDelete();
  virtual void gfxUpload(gfx::Engine* engine);

  virtual void onLoadData(common::OptionalData data);

  virtual Type getType() { return BaseResource::Texture; }
  gfx::BaseTexture* getTexture();
};

class Model : public BaseGfxResource {
  std::vector<gfx::Mesh> gfxMesh;
  std::vector<resource::Texture*> textures;

 public:
  Model(ResourceManager* rm, std::string name);

  virtual void gfxDelete();
  virtual void gfxUpload(gfx::Engine* engine);

  virtual void onLoadData(common::OptionalData data);
  virtual Type getType() { return BaseResource::Model; }
};

};  // namespace resource
}  // namespace rdm
