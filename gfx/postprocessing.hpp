#pragma once
#include <memory>

#include "base_device.hpp"
#include "base_types.hpp"

#define NR_GBUFFER_TEXTURES 4

namespace rdm::gfx {
class Engine;
class Viewport;
class PostProcessingManager {
  Engine* engine;
  bool reset;

  std::unique_ptr<BaseFrameBuffer> pingpongFramebuffer[2];
  std::unique_ptr<BaseTexture> pingpongTexture[2];
  std::unique_ptr<BaseFrameBuffer> resultFramebuffer;
  std::unique_ptr<BaseTexture> resultFramebufferTextures[2];
  bool bloomEnabled;
  bool noDF;

 public:
  PostProcessingManager(Engine* engine);

  void updateResolution(Viewport* vp);
  void runEffects(Viewport* vp);
  void draw(Viewport* vp, glm::vec2 resolution);
  void prepare(BaseDevice* device);
  void applyRenderState(BaseDevice* device);
};
};  // namespace rdm::gfx
