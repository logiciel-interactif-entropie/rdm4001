#include "engine.hpp"

#include <cstdio>
#include <stdexcept>

#include "filesystem.hpp"
#include "game.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gl_device.hpp"
#include "logging.hpp"
#include "scheduler.hpp"
#include "settings.hpp"
#include "world.hpp"

#ifndef DISABLE_EASY_PROFILER
#include <easy/profiler.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace rdm::gfx {
TextureCache::TextureCache(BaseDevice* device) {
  this->device = device;
  this->invalidTexture = device->createTexture();
}

std::optional<std::pair<TextureCache::Info, BaseTexture*>>
TextureCache::getOrLoad2d(const char* path, bool keepData) {
  auto it = textures.find(path);
  if (it == textures.end()) {
    TextureCache::Info i;
    common::OptionalData data = common::FileSystem::singleton()->readFile(path);
    if (data) {
      stbi_set_flip_vertically_on_load(true);
      stbi_uc* uc = stbi_load_from_memory(data->data(), data->size(), &i.width,
                                          &i.height, &i.channels, 0);
      if (uc) {
        switch (i.channels) {
          case 3:
            i.format = BaseTexture::RGB;
            i.internalFormat = BaseTexture::RGB8;
            break;
          case 4:
            i.format = BaseTexture::RGBA;
            i.internalFormat = BaseTexture::RGBA8;
            break;
        }

        std::unique_ptr<BaseTexture> tx = device->createTexture();
        tx->upload2d(i.width, i.height, DtUnsignedByte, i.format, uc, 4);
        textures[path] =
            std::pair<TextureCache::Info, std::unique_ptr<BaseTexture>>(
                i, std::move(tx));

        if (keepData) {
          i.data = uc;
        } else {
          i.data = 0;
          stbi_image_free(uc);
        }

        return std::pair<TextureCache::Info, BaseTexture*>(
            i, textures[path].second.get());
      } else {
        throw std::runtime_error("stbi_load_from_memory");
      }
    } else {
      return {};
    }
  } else {
    return std::pair<TextureCache::Info, BaseTexture*>(it->second.first,
                                                       it->second.second.get());
  }
}

std::optional<std::pair<TextureCache::Info, BaseTexture*>> TextureCache::get(
    const char* path) {
  auto it = textures.find(path);
  if (it == textures.end()) {
    return {};
  } else {
    return std::pair<TextureCache::Info, BaseTexture*>(it->second.first,
                                                       it->second.second.get());
  }
}

BaseTexture* TextureCache::cacheExistingTexture(
    const char* path, std::unique_ptr<BaseTexture>& texture, Info info) {
  auto it = textures.find(path);
  if (it == textures.end()) {
    textures[path] =
        std::pair<TextureCache::Info, std::unique_ptr<BaseTexture>>(
            info, std::move(texture));
    return textures[path].second.get();
  } else {
    rdm::Log::printf(
        LOG_ERROR,
        "Attempt to cache existing texture %s even though it is already cached",
        path);
    throw std::runtime_error("Texture already exists");
  }
}

BaseTexture* TextureCache::createCacheTexture(const char* path, Info info) {
  auto it = textures.find(path);
  if (it == textures.end()) {
    textures[path] =
        std::pair<TextureCache::Info, std::unique_ptr<BaseTexture>>(
            info, device->createTexture());
    return textures[path].second.get();
  } else {
    throw std::runtime_error("Texture already exists");
  }
}

void TextureCache::deleteTexture(const char* path) { textures.erase(path); }

static CVar r_bloomamount("r_bloomamount", "10", CVARF_SAVE | CVARF_GLOBAL);
static CVar r_rate("r_rate", "60.0", CVARF_SAVE | CVARF_GLOBAL);
static CVar r_bloom("r_bloom", "1", CVARF_SAVE | CVARF_GLOBAL);
static CVar r_scale("r_scale", "1.0", CVARF_SAVE | CVARF_GLOBAL);

class RenderJob : public SchedulerJob {
  Engine* engine;

 public:
  RenderJob(Engine* engine) : SchedulerJob("Render"), engine(engine) {}

  virtual double getFrameRate() {
    double renderFr = r_rate.getFloat();
    if (renderFr == 0.0) return 0.0;
    return 1.0 / renderFr;
  }

  virtual void startup() { engine->context->setCurrent(); }

  virtual void shutdown() { engine->context->unsetCurrent(); }

  virtual Result step() {
#ifndef DISABLE_EASY_PROFILER
    EASY_FUNCTION();
#endif
    BaseDevice* device = engine->device.get();

    bool bloomEnabled = r_bloom.getBool();

    try {
      engine->time = getStats().time;

      glm::ivec2 bufSize = engine->getContext()->getBufferSize();
      static bool lastBloom = false;
      static float lastScale = 1.0;
      if (engine->windowResolution != bufSize ||
          lastScale != r_scale.getFloat() || lastBloom != r_bloom.getBool()) {
        lastBloom = r_bloom.getBool();
        lastScale = r_scale.getFloat();
        engine->windowResolution = bufSize;
        engine->initializeBuffers(bufSize, true);
      }

#ifndef DISABLE_EASY_PROFILER
      EASY_BLOCK("Setup Frame");
#endif

      engine->getWorld()->getGame()->getResourceManager()->tickGfx(engine);

#ifndef DISABLE_EASY_PROFILER
      EASY_END_BLOCK;
#endif

#ifndef DISABLE_EASY_PROFILER
      EASY_BLOCK("Setup Frame");
#endif
      void* _ =
          engine->device->bindFramebuffer(engine->postProcessFrameBuffer.get());

      {
        BaseFrameBuffer::AttachmentPoint drawBuffers[] = {
            BaseFrameBuffer::Color0,
            BaseFrameBuffer::Color1,
        };

        // clear buffers
        device->targetAttachments(&drawBuffers[0], 1);
        device->clear(engine->clearColor.x, engine->clearColor.y,
                      engine->clearColor.z, 0.0);
        if (bloomEnabled) {
          device->targetAttachments(&drawBuffers[1], 1);
          device->clear(0.0, 0.0, 0.0, 0.0);

          device->targetAttachments(drawBuffers, 2);
        } else {
          device->targetAttachments(drawBuffers, 1);
        }
      }

      device->viewport(0, 0, engine->targetResolution.x,
                       engine->targetResolution.y);
      device->clearDepth();
      device->setDepthState(BaseDevice::LEqual);
      device->setCullState(BaseDevice::FrontCW);

      engine->getCamera().updateCamera(
          glm::vec2(engine->targetResolution.x, engine->targetResolution.y));
#ifndef DISABLE_EASY_PROFILER
      EASY_END_BLOCK;
#endif

      if (!engine->isInitialized) engine->initialize();

      try {
        engine->render();
      } catch (std::exception& e) {
        Log::printf(LOG_ERROR, "Error in engine->render(), e.what() = %s",
                    e.what());
      }

      engine->device->unbindFramebuffer(_);

#ifndef DISABLE_EASY_PROFILER
      EASY_BLOCK("Render Draw Buffer");
#endif

      device->setDepthState(BaseDevice::Always);
      device->setCullState(BaseDevice::None);

      if (bloomEnabled) {
#ifndef DISABLE_EASY_PROFILER
        EASY_BLOCK("Bloom");
#endif

        {
          bool horizontal = true, firstIteration = true;
          int amount = r_bloomamount.getInt();
          std::shared_ptr<gfx::Material> material =
              engine->getMaterialCache()->getOrLoad("GaussianBlur").value();
          for (int i = 0; i < amount; i++) {
            void* framebuffer = engine->getDevice()->bindFramebuffer(
                engine->pingpongFramebuffer[horizontal].get());
            engine->renderFullscreenQuad(
                NULL, material.get(),
                [this, horizontal, firstIteration](BaseProgram* program) {
                  program->setParameter(
                      "horizontal", DtInt,
                      BaseProgram::Parameter{.integer = horizontal});
                  program->setParameter(
                      "image", DtSampler,
                      BaseProgram::Parameter{
                          .texture.slot = 0,
                          .texture.texture =
                              firstIteration
                                  ? engine->fullscreenTextureBloom.get()
                                  : engine->pingpongTexture[!horizontal]
                                        .get()});
                });
            horizontal = !horizontal;
            if (firstIteration) firstIteration = false;
            engine->getDevice()->unbindFramebuffer(framebuffer);
          }
        }

#ifndef DISABLE_EASY_PROFILER
        EASY_END_BLOCK;
#endif
      }

      device->viewport(0, 0, engine->windowResolution.x,
                       engine->windowResolution.y);
      engine->renderFullscreenQuad(
          engine->fullscreenTexture.get(), NULL, [this](BaseProgram* p) {
            if (r_bloom.getBool())
              p->setParameter(
                  "texture1", DtSampler,
                  BaseProgram::Parameter{
                      .texture.slot = 1,
                      .texture.texture = engine->pingpongTexture[1].get()});
            p->setParameter(
                "bloom", DtInt,
                BaseProgram::Parameter{.integer = r_bloom.getBool()});
            p->setParameter(
                "target_res", DtVec2,
                BaseProgram::Parameter{.vec2 = engine->targetResolution});
            p->setParameter(
                "window_res", DtVec2,
                BaseProgram::Parameter{.vec2 = engine->windowResolution});
            p->setParameter(
                "forced_aspect", DtFloat,
                BaseProgram::Parameter{.number = (float)engine->forcedAspect});
          });
#ifndef DISABLE_EASY_PROFILER
      EASY_END_BLOCK;
#endif
    } catch (std::exception& e) {
      std::scoped_lock lock(engine->context->getMutex());
      Log::printf(LOG_ERROR, "Error in render: %s", e.what());
    }

#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("Gui");
#endif
    engine->gui->render();
#ifndef DISABLE_EASY_PROFILER
    EASY_END_BLOCK;
#endif

#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("ImGui");
#endif
    engine->device->stopImGui();
#ifndef DISABLE_EASY_PROFILER
    EASY_END_BLOCK;
#endif
    engine->imguiLock.unlock();

    engine->afterGuiRenderStepped.fire();

#ifndef DISABLE_EASY_PROFILER
    EASY_BLOCK("Swap Buffers");
#endif
    engine->context->swapBuffers();
#ifndef DISABLE_EASY_PROFILER
    EASY_END_BLOCK;
#endif

    return Stepped;
  }
};

static CVar r_samples("r_samples", "1", CVARF_GLOBAL | CVARF_SAVE);

Engine::Engine(World* world, void* hwnd) {
  fullscreenSamples = r_samples.getInt();
  maxFbScale = 1.0;
  forcedAspect = 0.0;
  context.reset(new gl::GLContext(hwnd));
  std::scoped_lock lock(context->getMutex());
  device.reset(new gl::GLDevice(dynamic_cast<gl::GLContext*>(context.get())));
  device->engine = this;
  textureCache.reset(new TextureCache(device.get()));
  materialCache.reset(new MaterialCache(device.get()));
  meshCache.reset(new MeshCache(this));

  clearColor = glm::vec3(0.3, 0.3, 0.3);

  fullscreenMaterial =
      materialCache->getOrLoad("PostProcess").value_or(nullptr);
  if (fullscreenMaterial) {
    initializeBuffers(glm::vec2(1.0, 1.0), false);

  } else {
    throw std::runtime_error("Could not load PostProcess material!!!");
  }

  renderJob = world->getScheduler()->addJob(new RenderJob(this));
  world->stepped.listen([this] { stepped(); });
  isInitialized = false;

  this->world = world;
}

void Engine::renderFullscreenQuad(
    BaseTexture* texture, Material* material,
    std::function<void(BaseProgram*)> setParameters) {
  if (material == 0) material = fullscreenMaterial.get();
  BaseProgram* fullscreenProgram = material->prepareDevice(device.get(), 0);
  if (fullscreenProgram) {
    if (texture)
      fullscreenProgram->setParameter(
          "texture0", DtSampler,
          BaseProgram::Parameter{.texture.slot = 0,
                                 .texture.texture = texture});
    setParameters(fullscreenProgram);
    fullscreenProgram->bind();
  }
  fullScreenArrayPointers->bind();
  device->draw(fullscreenBuffer.get(), DtFloat, BaseDevice::Triangles, 3);
}

void Engine::setFullscreenMaterial(const char* name) {
  fullscreenMaterial = materialCache->getOrLoad(name).value_or(nullptr);
}

void Engine::initializeBuffers(glm::vec2 res, bool reset) {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif

  // set up buffers for post processing/hdr
  if (!reset) {
    postProcessFrameBuffer = device->createFrameBuffer();
    fullscreenBuffer = device->createBuffer();
    fullscreenTexture = device->createTexture();
    fullscreenTextureDepth = device->createTexture();
    fullscreenTextureBloom = device->createTexture();
    fullScreenArrayPointers = device->createArrayPointers();
    fullScreenArrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DataType::DtVec2, 0, 3, 0, 0, fullscreenBuffer.get()));
    fullscreenBuffer->upload(BaseBuffer::Array, BaseBuffer::StaticDraw,
                             sizeof(float) * 6,
                             (float[]){0.0, 0.0, 2.0, 0.0, 0.0, 2.0});
  } else {
    postProcessFrameBuffer->destroyAndCreate();
    fullscreenTexture->destroyAndCreate();
    fullscreenTextureDepth->destroyAndCreate();
    fullscreenTextureBloom->destroyAndCreate();
  }

  glm::vec2 fbSizeF = res;
  double s = std::min(maxFbScale, (double)r_scale.getFloat());
  fbSizeF *= s;
  if (forcedAspect != 0.0) {
    fbSizeF.x = fbSizeF.y * forcedAspect;
  }
  fbSizeF = glm::max(fbSizeF, glm::vec2(1, 1));
  targetResolution = fbSizeF;

  try {
    // set resolutions of buffers
    fullscreenTexture->reserve2dMultisampled(
        fbSizeF.x, fbSizeF.y, BaseTexture::RGBAF32, fullscreenSamples);

    postProcessFrameBuffer->setTarget(fullscreenTexture.get());

    fullscreenTextureDepth->reserve2dMultisampled(
        fbSizeF.x, fbSizeF.y, BaseTexture::D24S8, fullscreenSamples, true);

    postProcessFrameBuffer->setTarget(fullscreenTextureDepth.get(),
                                      BaseFrameBuffer::DepthStencil);

    if (r_bloom.getBool()) {
      fullscreenTextureBloom->reserve2dMultisampled(
          fbSizeF.x, fbSizeF.y, BaseTexture::RGBAF32, fullscreenSamples);

      postProcessFrameBuffer->setTarget(fullscreenTextureBloom.get(),
                                        BaseFrameBuffer::Color1);
    }

    // set up ping pong buffers for gaussian blur
    for (int i = 0; i < 2; i++) {
      if (!reset) {
        pingpongFramebuffer[i] = device->createFrameBuffer();
        pingpongTexture[i] = device->createTexture();
      } else {
        pingpongFramebuffer[i]->destroyAndCreate();
        pingpongTexture[i]->destroyAndCreate();
      }

      if (r_bloom.getBool()) {
        pingpongTexture[i]->reserve2dMultisampled(
            fbSizeF.x, fbSizeF.y, BaseTexture::RGBAF32, fullscreenSamples);
        pingpongFramebuffer[i]->setTarget(pingpongTexture[i].get());
      }
    }

    if (postProcessFrameBuffer->getStatus() != BaseFrameBuffer::Complete) {
      Log::printf(LOG_ERROR, "BaseFrameBuffer::getStatus() = %i",
                  postProcessFrameBuffer->getStatus());
    }
  } catch (std::exception& e) {
    Log::printf(LOG_ERROR, "Creating post fb: %s", e.what());
  }
  Log::printf(LOG_DEBUG, "Updating size of framebuffer to (%f,%f)", fbSizeF.x,
              fbSizeF.y);
  if (postProcessFrameBuffer->getStatus() != BaseFrameBuffer::Complete) {
    Log::printf(LOG_ERROR, "BaseFrameBuffer::getStatus() = %i",
                postProcessFrameBuffer->getStatus());
  }
}

void Engine::stepped() {}

void Engine::render() {
#ifndef DISABLE_EASY_PROFILER
  EASY_FUNCTION();
#endif

  imguiLock.lock();
  device->startImGui();

  renderStepped.fire();
  for (int i = 0; i < entities.size(); i++) {
    Entity* ent = entities[i].get();
    try {
      ent->render(device.get());
    } catch (std::exception& error) {
      Log::printf(LOG_ERROR, "Error rendering entity %i", i);
    }
  }

  const char* passName[] = {
      "Opaque",
      "Transparent",
  };
  for (int i = 0; i < RenderPass::_Max; i++) {
    device->dbgPushGroup(passName[i]);
    passes[i].render(this);
    device->dbgPopGroup();
  }

  device->setDepthState(BaseDevice::Disabled);
  device->setCullState(BaseDevice::None);

  if (getWorld()->getPhysicsWorld())
    if (getWorld()->getPhysicsWorld()->isDebugDrawEnabled()) {
      if (!getWorld()->getPhysicsWorld()->isDebugDrawInitialized()) {
        getWorld()->getPhysicsWorld()->initializeDebugDraw(this);
      }
      getWorld()->getPhysicsWorld()->getWorld()->debugDrawWorld();
      afterDebugDrawRenderStepped.fire();
    }

  afterRenderStepped.fire();

  device->setDepthState(BaseDevice::LEqual);
  device->setCullState(BaseDevice::FrontCW);
}

void Engine::initialize() {
  gui = std::unique_ptr<gui::GuiManager>(new gui::GuiManager(this));
  isInitialized = true;

  initialized.fire();
}

Entity* Engine::addEntity(std::unique_ptr<Entity> entity) {
  entities.push_back(std::move(entity));
  return (entities.back()).get();
}

void Engine::deleteEntity(Entity* entity) {
  for (int i = 0; i < entities.size(); i++)
    if (entities[i].get() == entity) {
      entities.erase(entities.begin() + i);
      break;
    }
}
}  // namespace rdm::gfx
