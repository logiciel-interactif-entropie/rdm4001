#pragma once
#include <memory>

#include "base_types.hpp"
#include "camera.hpp"
#include "lighting.hpp"
#include "postprocessing.hpp"
namespace rdm::gfx {
class Engine;

struct ViewportGfxSettings {
  int msaaSamples;
  int numColorBuffers;
  BaseTexture::InternalFormat format;
  glm::ivec2 resolution;
  bool canBloomBeEnabled;
  bool wantNoDeferred;
  int maxBloomPasses;

  ViewportGfxSettings() {
    msaaSamples = 0;
    numColorBuffers = 1;
    format = BaseTexture::RGBAF32;
    resolution = glm::ivec2(100, 100);
    canBloomBeEnabled = false;
    maxBloomPasses = 10;
    wantNoDeferred = true;
  }

  bool operator==(const ViewportGfxSettings& other) {
    if (other.msaaSamples != msaaSamples) return false;
    if (other.numColorBuffers != numColorBuffers) return false;
    if (other.resolution != resolution) return false;
    if (other.format != format) return false;
    if (other.canBloomBeEnabled != canBloomBeEnabled) return false;
    if (other.maxBloomPasses != maxBloomPasses) return false;
    if (other.wantNoDeferred != wantNoDeferred) return false;
    return true;
  }
};

class Viewport {
  LightingManager lightingSystem;
  PostProcessingManager postProcessing;
  std::vector<std::unique_ptr<BaseTexture>> colorBuffers;
  std::unique_ptr<BaseTexture> depthBuffer;
  std::unique_ptr<BaseFrameBuffer> framebuffer;
  gfx::Engine* engine;
  Camera frameCamera;
  Camera camera;

  void updateBuffers(bool firstTime);

  ViewportGfxSettings settings;

 public:
  Viewport(gfx::Engine* engine,
           ViewportGfxSettings settings = ViewportGfxSettings());

  void* bind();
  void unbind(void* _);

  BaseTexture* get(int colorBuffer = 0) {
    return colorBuffers[colorBuffer].get();
  };

  BaseFrameBuffer* getFb() { return framebuffer.get(); }

  ViewportGfxSettings getSettings() { return settings; }
  void updateSettings(ViewportGfxSettings settings);

  void applyRenderState();

  glm::vec2 project(glm::vec3 p);

  LightingManager& getLightingManager() { return lightingSystem; }
  Camera& getCamera() { return camera; }
  Camera getFrameCamera() { return frameCamera; }

  PostProcessingManager* getPostProcessingManager() { return &postProcessing; }

  void draw(glm::vec2 resolution);
};
}  // namespace rdm::gfx
