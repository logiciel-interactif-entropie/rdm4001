#pragma once
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base_context.hpp"
#include "base_device.hpp"
#include "camera.hpp"
#include "entity.hpp"
#include "gfx/base_types.hpp"
#include "gfx/gui/gui.hpp"
#include "gfx/mesh.hpp"
#include "gfx/video.hpp"
#include "renderpass.hpp"
#include "scheduler.hpp"
#include "signal.hpp"
#include "video.hpp"
#include "viewport.hpp"

namespace rdm {
class World;
}  // namespace rdm

namespace rdm::gfx {
class TextureCache {
  std::unique_ptr<BaseTexture> invalidTexture;
  BaseDevice* device;

 public:
  struct Info {
    BaseTexture::InternalFormat internalFormat;
    BaseTexture::Format format;
    int width;
    int height;
    int channels;

    unsigned char* data;
  };

  TextureCache(BaseDevice* device);
  std::optional<std::pair<Info, BaseTexture*>> getOrLoad2d(
      const char* path, bool keepData = false);
  std::optional<std::pair<Info, BaseTexture*>> get(const char* path);
  BaseTexture* cacheExistingTexture(const char* path,
                                    std::unique_ptr<BaseTexture>& texture,
                                    Info info);
  BaseTexture* createCacheTexture(const char* path, Info info);
  void deleteTexture(const char* path);

 private:
  std::map<std::string, std::pair<Info, std::unique_ptr<BaseTexture>>> textures;
};

class Engine {
  friend class RenderJob;
  SchedulerJob* renderJob;

  std::unique_ptr<BaseContext> context;
  std::unique_ptr<BaseDevice> device;
  std::unique_ptr<gui::GuiManager> gui;
  std::unique_ptr<VideoRenderer> videoRenderer;
  std::vector<std::unique_ptr<Entity>> entities;

  std::unique_ptr<Viewport> viewport;

  std::unique_ptr<BaseBuffer> fullscreenBuffer;
  std::unique_ptr<BaseArrayPointers> fullScreenArrayPointers;
  std::shared_ptr<Material> fullscreenMaterial;

  std::unique_ptr<BaseFrameBuffer> pingpongFramebuffer[2];
  std::unique_ptr<BaseTexture> pingpongTexture[2];

  int fullscreenSamples;

  float time;

  glm::vec3 clearColor;

  bool isInitialized;
  void initialize();
  void render();
  void stepped();

  void initializeBuffers(glm::vec2 res, bool reset);

  std::unique_ptr<MaterialCache> materialCache;
  std::unique_ptr<TextureCache> textureCache;
  std::unique_ptr<MeshCache> meshCache;
  World* world;

  glm::ivec2 windowResolution;
  glm::vec2 targetResolution;
  std::mutex imguiLock;

  double maxFbScale;
  double forcedAspect;

  RenderPass passes[RenderPass::_Max];

  Viewport* currentViewport;
  void* vpRef;

 public:
  Engine(World* world, void* hwnd);

  std::mutex& getImguiLock() { return imguiLock; }

  RenderPass& pass(RenderPass::Pass pass) { return passes[pass]; };

  World* getWorld() { return world; }

  void renderFullscreenQuad(
      BaseTexture* texture, Material* material = 0,
      std::function<void(BaseProgram*)> setParameters = [](BaseProgram*) {});
  void setFullscreenMaterial(const char* name);

  /**
   * @brief This signal will be fired in the Render job thread.
   *
   * It will fire after all Entity's have rendered, and before it unbinds the
   * post-process framebuffer.
   */
  Signal<> renderStepped;
  Signal<> afterRenderStepped;
  Signal<> afterGuiRenderStepped;
  Signal<> afterDebugDrawRenderStepped;
  Signal<> initialized;

  void setMaxFbScale(double d) { maxFbScale = d; }
  void setForcedAspect(double d) { forcedAspect = d; }

  /**
   * @brief Use Engine::addEntity<T>();
   */
  Entity* addEntity(std::unique_ptr<Entity> entity);
  void deleteEntity(Entity* entity);
  SchedulerJob* getRenderJob() { return renderJob; }

  template <typename T, class... Types>
  T* addEntity(Types... args) {
    static_assert(std::is_base_of<Entity, T>::value,
                  "T must derive from Entity");
    T* t = (T*)addEntity(std::unique_ptr<Entity>(new T(args...)));
    t->initialize();
    return t;
  }

  void setClearColor(glm::vec3 color) { clearColor = color; }

  glm::vec2 getTargetResolution() { return targetResolution; }
  glm::vec2 getWindowResolution() { return windowResolution; }

  BaseContext* getContext() { return context.get(); }
  BaseDevice* getDevice() { return device.get(); }
  MaterialCache* getMaterialCache() { return materialCache.get(); }
  TextureCache* getTextureCache() { return textureCache.get(); }
  [[deprecated("Use ResourceManager and the Model type")]]
  MeshCache* getMeshCache() {
    return meshCache.get();
  }
  VideoRenderer* getVideoRenderer() { return videoRenderer.get(); }
  gui::GuiManager* getGuiManager() { return gui.get(); }

  float getTime() { return time; }

  Viewport* getMainViewport() { return viewport.get(); }
  Viewport* getCurrentViewport() {
    return currentViewport ? currentViewport : getMainViewport();
  };

  gfx::Camera& getCamera() { return getCurrentViewport()->getCamera(); }

  void* setViewport(Viewport* viewport);
  void finishViewport(void* _);
};
}  // namespace rdm::gfx
