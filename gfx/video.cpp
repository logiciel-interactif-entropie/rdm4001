#include "video.hpp"

#include "engine.hpp"
namespace rdm::gfx {
VideoRenderer::VideoRenderer(gfx::Engine* engine) { this->engine = engine; }

VideoRenderer::~VideoRenderer() {}

void VideoRenderer::play(const char* path) {}

void VideoRenderer::render() {
  if (currentStatus == Status::Playing) {
    engine->renderFullscreenQuad(videoTexture.get());
  }
}
}  // namespace rdm::gfx
