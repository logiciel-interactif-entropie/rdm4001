#include "postprocessing.hpp"

#include <format>

#include "base_device.hpp"
#include "base_types.hpp"
#include "engine.hpp"
#include "logging.hpp"
#include "settings.hpp"
#include "viewport.hpp"
namespace rdm::gfx {
PostProcessingManager::PostProcessingManager(Engine* engine) {
  this->engine = engine;
  resultFramebuffer = engine->getDevice()->createFrameBuffer();
  for (int i = 0; i < 2; i++) {
    pingpongFramebuffer[i] = engine->getDevice()->createFrameBuffer();
    pingpongFramebuffer[i]->setTag(std::format("PP Fb {}", i));
    pingpongTexture[i] = engine->getDevice()->createTexture();
    resultFramebufferTextures[i] = engine->getDevice()->createTexture();
    pingpongFramebuffer[i]->setTag(std::format("RF Fb {}", i));
  }
  reset = false;
}

static CVar r_bloomamount("r_bloomamount", "10", CVARF_SAVE | CVARF_GLOBAL);
static CVar r_bloom("r_bloom", "1", CVARF_SAVE | CVARF_GLOBAL);

void PostProcessingManager::prepare(BaseDevice* device) {
  BaseFrameBuffer::AttachmentPoint drawBuffers[] = {
      BaseFrameBuffer::Color0,
      BaseFrameBuffer::Color1,
      BaseFrameBuffer::Color2,
      BaseFrameBuffer::Color3,
  };

  if (!noDF) {
    void* _ = device->bindFramebuffer(resultFramebuffer.get());
    device->targetAttachments(&drawBuffers[1], 1);
    device->clear(0.f, 0.f, 0.f, 0.f);
    device->targetAttachments(&drawBuffers[0], 1);
    device->clear(0.f, 0.f, 0.f, 1.f);
    device->clearDepth();
    device->unbindFramebuffer(_);
  }

  device->targetAttachments(
      drawBuffers, noDF ? (bloomEnabled ? 2 : 1) : NR_GBUFFER_TEXTURES);
  device->clear(0.f, 0.f, 0.f, 0.0);
  device->clearDepth();

  device->setBlendState(BaseDevice::DDisabled, BaseDevice::DDisabled);
  device->setDepthState(BaseDevice::LEqual);
  device->setCullState(BaseDevice::FrontCW);

  engine->getMaterialCache()->setPreferNoDF(noDF);
}

void PostProcessingManager::applyRenderState(BaseDevice* device) {
  BaseFrameBuffer::AttachmentPoint drawBuffers[] = {
      BaseFrameBuffer::Color0,
      BaseFrameBuffer::Color1,
      BaseFrameBuffer::Color2,
      BaseFrameBuffer::Color3,
  };

  device->targetAttachments(
      drawBuffers, noDF ? (bloomEnabled ? 2 : 1) : NR_GBUFFER_TEXTURES);

  device->setBlendState(BaseDevice::DDisabled, BaseDevice::DDisabled);
  device->setDepthState(BaseDevice::LEqual);
  device->setCullState(BaseDevice::FrontCW);

  engine->getMaterialCache()->setPreferNoDF(noDF);
}

void PostProcessingManager::updateResolution(Viewport* vp) {
  ViewportGfxSettings settings = vp->getSettings();
  int fullscreenSamples = settings.msaaSamples;
  glm::vec2 fbSizeF = settings.resolution;

  try {
    // set up ping pong buffers for gaussian blur
    for (int i = 0; i < 2; i++) {
      if (reset) {
        pingpongFramebuffer[i]->destroyAndCreate();
        pingpongTexture[i]->destroyAndCreate();
      }

      if (settings.canBloomBeEnabled && r_bloom.getBool()) {
        if (fullscreenSamples) {
          pingpongTexture[i]->reserve2dMultisampled(
              fbSizeF.x, fbSizeF.y, BaseTexture::RGBAF32, fullscreenSamples);
          pingpongFramebuffer[i]->setTarget(pingpongTexture[i].get());

        } else {
          pingpongTexture[i]->reserve2d(fbSizeF.x, fbSizeF.y,
                                        BaseTexture::RGBAF32);
          pingpongFramebuffer[i]->setTarget(pingpongTexture[i].get());
        }
        bloomEnabled = true;
      } else {
        bloomEnabled = false;
      }
    }

    // set up result framebuffer
    if (reset) {
      resultFramebuffer->destroyAndCreate();
    }

    for (int i = 0; i < 2; i++) {
      if (reset) {
        resultFramebufferTextures[i]->destroyAndCreate();
      }
    }

    noDF = false;
    if (settings.numColorBuffers != NR_GBUFFER_TEXTURES) {
      noDF = true;
    }
    if (settings.wantNoDeferred) {
      noDF = true;
    }

    if (!noDF) {
      resultFramebufferTextures[0]->reserve2d(fbSizeF.x, fbSizeF.y,
                                              BaseTexture::RGBAF32);
      resultFramebuffer->setTarget(resultFramebufferTextures[0].get(),
                                   BaseFrameBuffer::Color0);

      if (bloomEnabled) {
        resultFramebufferTextures[1]->reserve2d(fbSizeF.x, fbSizeF.y,
                                                BaseTexture::RGBAF32);
        resultFramebuffer->setTarget(resultFramebufferTextures[1].get(),
                                     BaseFrameBuffer::Color1);
      }

      if (resultFramebuffer->getStatus() != BaseFrameBuffer::Complete) {
        Log::printf(LOG_ERROR, "BaseFrameBuffer::getStatus() = %i",
                    resultFramebuffer->getStatus());
        throw std::runtime_error("Failed creating Viewport framebuffer");
      }
    }
  } catch (std::exception& e) {
    Log::printf(LOG_ERROR, "Creating post fb: %s", e.what());
  }

  reset = true;
}

void PostProcessingManager::runEffects(Viewport* vp) {
  Profiler& profiler = engine->getRenderJob()->getProfiler();

  void* _ = engine->getDevice()->bindFramebuffer(
      noDF ? vp->getFb() : resultFramebuffer.get());
  BaseFrameBuffer::AttachmentPoint drawBuffers[] = {
      BaseFrameBuffer::Color0,
      BaseFrameBuffer::Color1,
      BaseFrameBuffer::Color2,
      BaseFrameBuffer::Color3,
  };
  engine->getDevice()->targetAttachments(drawBuffers, 2);

  if (!noDF) {
    profiler.fun("Post process: G-Buffer");
    std::shared_ptr<gfx::Material> deferredRenderingMaterial =
        engine->getMaterialCache()->getOrLoad("DeferredRenderingPost").value();
    engine->renderFullscreenQuad(
        NULL, deferredRenderingMaterial.get(), [vp](BaseProgram* program) {
          program->setParameter(
              "gbuffer_position", DtSampler,
              {.texture = {.slot = 0, .texture = vp->get(0)}});
          program->setParameter(
              "gbuffer_normal", DtSampler,
              {.texture = {.slot = 1, .texture = vp->get(1)}});
          program->setParameter(
              "gbuffer_albedo", DtSampler,
              {.texture = {.slot = 2, .texture = vp->get(2)}});
          program->setParameter(
              "gbuffer_material_properties", DtSampler,
              {.texture = {.slot = 3, .texture = vp->get(3)}});
        });
    profiler.end();
  }

  if (bloomEnabled) {
    profiler.fun("Post process: Bloom");
    bool horizontal = true, firstIteration = true;
    int amount =
        std::min(vp->getSettings().maxBloomPasses, r_bloomamount.getInt());
    std::shared_ptr<gfx::Material> material =
        engine->getMaterialCache()->getOrLoad("GaussianBlur").value();
    for (int i = 0; i < amount; i++) {
      void* framebuffer = engine->getDevice()->bindFramebuffer(
          pingpongFramebuffer[horizontal].get());
      engine->renderFullscreenQuad(
          NULL, material.get(),
          [this, horizontal, firstIteration, vp](BaseProgram* program) {
            program->setParameter(
                "horizontal", DtInt,
                BaseProgram::Parameter{.integer = horizontal});
            program->setParameter(
                "image", DtSampler,
                BaseProgram::Parameter{
                    .texture = {
                        .slot = 0,
                        .texture =
                            firstIteration
                                ? (!noDF ? vp->get(1)
                                         : resultFramebufferTextures[1].get())
                                : pingpongTexture[!horizontal].get()}});
          });
      horizontal = !horizontal;
      if (firstIteration) firstIteration = false;
      engine->getDevice()->unbindFramebuffer(framebuffer);
      profiler.end();
    }
  }
  engine->getDevice()->unbindFramebuffer(_);
}

void PostProcessingManager::draw(Viewport* vp, glm::vec2 resolution) {
  Profiler& profiler = engine->getRenderJob()->getProfiler();
  int fullscreenSamples = vp->getSettings().msaaSamples;
  profiler.fun("Post process: Draw");
  engine->renderFullscreenQuad(
      noDF ? vp->get(0) : resultFramebufferTextures[0].get(), NULL,
      [this, fullscreenSamples, vp, resolution](BaseProgram* p) {
        bool multisampling = fullscreenSamples != 0;
        p->setParameter(
            "texture1", DtSampler,
            BaseProgram::Parameter{
                .texture = {
                    .slot = 1,
                    .texture = bloomEnabled
                                   ? (noDF ? vp->get(1)
                                           : resultFramebufferTextures[1].get())
                                   : NULL}});
        p->setParameter(
            "texture2", DtSampler,
            BaseProgram::Parameter{
                .texture = {.slot = 2,
                            .texture = engine->getGuiViewport()->get()}});
        p->setParameter("bloom", DtInt,
                        BaseProgram::Parameter{.integer = bloomEnabled});
        p->setParameter(
            "target_res", DtVec2,
            BaseProgram::Parameter{.vec2 = vp->getSettings().resolution});
        p->setParameter("window_res", DtVec2,
                        BaseProgram::Parameter{.vec2 = resolution});
        /*
          p->setParameter(
            "forced_aspect", DtFloat,
            BaseProgram::Parameter{.number = (float)engine->getForcedAspect()});
          p->setParameter("exposure", DtFloat,
            BaseProgram::Parameter{.number = r_exposure.getFloat()});
        */
        p->setParameter("samples", DtInt,
                        BaseProgram::Parameter{.integer = fullscreenSamples});
        vp->getLightingManager().upload(vp->getCamera().getPosition(), p);
      });
  profiler.end();
}
};  // namespace rdm::gfx
