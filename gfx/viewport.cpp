#include "viewport.hpp"

#include "base_types.hpp"
#include "engine.hpp"
namespace rdm::gfx {
Viewport::Viewport(gfx::Engine* engine, ViewportGfxSettings settings) {
  this->engine = engine;
  this->settings = settings;

  updateBuffers(true);

  camera.updateCamera(settings.resolution);
}

void Viewport::updateBuffers(bool firstTime) {
  if (firstTime) {
    framebuffer = engine->getDevice()->createFrameBuffer();
  } else {
    framebuffer->destroyAndCreate();
  }

  int numBuffers = firstTime ? settings.numColorBuffers : colorBuffers.size();
  for (int i = 0; i < numBuffers; i++) {
    if (firstTime) colorBuffers.push_back(engine->getDevice()->createTexture());
    auto& buffer = colorBuffers[i];
    if (!firstTime) buffer->destroyAndCreate();
    if (settings.msaaSamples) {
      buffer->reserve2dMultisampled(settings.resolution.x,
                                    settings.resolution.y, settings.format,
                                    settings.msaaSamples);
    } else {
      buffer->reserve2d(settings.resolution.x, settings.resolution.y,
                        settings.format);
    }

    framebuffer->setTarget(
        buffer.get(),
        (BaseFrameBuffer::AttachmentPoint)(BaseFrameBuffer::Color0 + i));
  }

  if (firstTime) {
    depthBuffer = engine->getDevice()->createTexture();
  } else {
    depthBuffer->destroyAndCreate();
  }

  if (settings.msaaSamples) {
    depthBuffer->reserve2dMultisampled(
        settings.resolution.x, settings.resolution.y, BaseTexture::D24S8,
        settings.msaaSamples, true);
  } else {
    depthBuffer->reserve2d(settings.resolution.x, settings.resolution.y,
                           BaseTexture::D24S8, settings.msaaSamples, true);
  }

  framebuffer->setTarget(depthBuffer.get(), BaseFrameBuffer::DepthStencil);

  if (framebuffer->getStatus() != BaseFrameBuffer::Complete) {
    Log::printf(LOG_ERROR, "BaseFrameBuffer::getStatus() = %i",
                framebuffer->getStatus());
    throw std::runtime_error("Failed creating Viewport framebuffer");
  }
}

void Viewport::updateSettings(ViewportGfxSettings settings) {
  if (settings != this->settings) {
    this->settings = settings;
    updateBuffers(false);
  }
}

void Viewport::applyRenderState() {
  std::vector<BaseFrameBuffer::AttachmentPoint> points;
  for (int i = 0; i < settings.numColorBuffers; i++) {
    points.push_back((BaseFrameBuffer::AttachmentPoint)i);
  }
  engine->getDevice()->targetAttachments(points.data(), points.size());
  engine->getDevice()->viewport(0, 0, settings.resolution.x,
                                settings.resolution.y);
}

void* Viewport::bind() {
  camera.updateCamera(glm::vec2(settings.resolution.x, settings.resolution.y));
  frameCamera = camera;
  return engine->getDevice()->bindFramebuffer(framebuffer.get());
}

void Viewport::unbind(void* _) { engine->getDevice()->unbindFramebuffer(_); }

glm::vec2 Viewport::project(glm::vec3 _p) {
  glm::vec4 p = camera.getProjectionMatrix() * camera.getViewMatrix() *
                glm::vec4(_p, 1.0);
  p.x /= p.w;
  p.y /= p.w;
  p.z /= p.w;
  p.x = (p.x + 1) * (float)settings.resolution.x * 0.5;
  p.y = (p.y + 1) * (float)settings.resolution.y * 0.5;
  if (p.z > 1.f) return glm::vec2(-1);
  if (p.z < -1.f) return glm::vec2(-1);
  return glm::vec2(p.x, p.y);
}
}  // namespace rdm::gfx
