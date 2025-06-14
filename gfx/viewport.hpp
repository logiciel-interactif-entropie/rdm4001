#pragma once
#include <memory>

#include "base_types.hpp"
#include "camera.hpp"
namespace rdm::gfx {
class Engine;

struct ViewportGfxSettings {
  int msaaSamples;
  int numColorBuffers;
  BaseTexture::InternalFormat format;
  glm::ivec2 resolution;

  ViewportGfxSettings() {
    msaaSamples = 0;
    numColorBuffers = 1;
    format = BaseTexture::RGBAF32;
    resolution = glm::ivec2(100, 100);
  }

  bool operator==(const ViewportGfxSettings& other) {
    if (other.msaaSamples != msaaSamples) return false;
    if (other.numColorBuffers != numColorBuffers) return false;
    if (other.resolution != resolution) return false;
    if (other.format != format) return false;
    return true;
  }
};

class Viewport {
  std::vector<std::unique_ptr<BaseTexture>> colorBuffers;
  std::unique_ptr<BaseTexture> depthBuffer;
  std::unique_ptr<BaseFrameBuffer> framebuffer;
  gfx::Engine* engine;
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

  Camera& getCamera() { return camera; }
};
}  // namespace rdm::gfx
