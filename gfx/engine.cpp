#include "engine.hpp"

#include <cstdio>
#include <stdexcept>

#include "apis.hpp"
#include "filesystem.hpp"
#include "game.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gl_context.hpp"
#include "gl_device.hpp"
#include "gui/ngui.hpp"
#include "imgui/imgui.h"
#include "logging.hpp"
#include "renderpass.hpp"
#include "scheduler.hpp"
#include "settings.hpp"
#include "video.hpp"
#include "viewport.hpp"
#ifdef RDM4001_FEATURE_VULKAN
#include "vk_context.hpp"
#include "vk_device.hpp"
#endif
#include "world.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace rdm::gfx {
#ifdef RDM4001_FEATURE_GLMODERN
GFX_API_INSTANTIATOR(GLModern, gl::GLContext, gl::GLDevice);
#endif

#ifdef RDM4001_FEATURE_VULKAN
GFX_API_INSTANTIATOR(Vulkan, vk::VKContext, vk::VKDevice);
#endif

static CVar r_disablepost("r_disablepost", "0", CVARF_GLOBAL | CVARF_SAVE);

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
static CVar r_exposure("r_exposure", "0.0", CVARF_GLOBAL);
static CVar r_samples("r_samples", "0", CVARF_GLOBAL | CVARF_SAVE);

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
    BaseDevice* device = engine->device.get();
    Profiler& profiler = engine->getRenderJob()->getProfiler();

    profiler.fun("step");

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
        ViewportGfxSettings settings = engine->viewport->getSettings();
        settings.numColorBuffers = r_bloom.getBool() ? 2 : 1;

        glm::vec2 fbSizeF = engine->windowResolution;
        double s = std::min(engine->maxFbScale, (double)r_scale.getFloat());
        fbSizeF *= s;
        if (engine->forcedAspect != 0.0) {
          fbSizeF.x = fbSizeF.y * engine->forcedAspect;
        }
        fbSizeF = glm::max(fbSizeF, glm::vec2(1, 1));
        engine->targetResolution = fbSizeF;
        settings.resolution = engine->targetResolution;

        ViewportGfxSettings guiSettings = engine->guiViewport->getSettings();
        guiSettings.resolution = engine->targetResolution;

        engine->initializeBuffers(bufSize, true);
        engine->viewport->updateSettings(settings);
        engine->guiViewport->updateSettings(settings);
      }

      engine->getWorld()->getGame()->getResourceManager()->tickGfx(engine);

      void* _;
      if (!r_disablepost.getBool()) _ = engine->viewport->bind();

      BaseFrameBuffer::AttachmentPoint drawBuffers[] = {
          BaseFrameBuffer::Color0,
          BaseFrameBuffer::Color1,
      };

      {
        // clear buffers
        device->targetAttachments(&drawBuffers[0], 1);
        device->clear(engine->clearColor.x, engine->clearColor.y,
                      engine->clearColor.z, 0.0);
        if (!r_disablepost.getBool() && bloomEnabled) {
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

      if (!engine->isInitialized) engine->initialize();

      try {
        engine->render();
      } catch (std::exception& e) {
        Log::printf(LOG_ERROR, "Error in engine->render(), e.what() = %s",
                    e.what());
      }

      /* TODO: Auto exposure feature
      struct {
        char r, g, b, a;
      } pixelData[100 * 100];
      engine->getDevice()->readPixels((engine->targetResolution.x / 2.f) - 100,
                                      (engine->targetResolution.y / 2.f) - 100,
                                      100, 100, &pixelData);
      float avg = 0.f;
      for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
          int p = i + (j * 100);
          float v = std::max(std::max(pixelData[p].r, pixelData[p].g),
                             pixelData[p].b);
          avg += v;
        }
      }
      rdm::Log::printf(LOG_DEBUG, "%f", avg);
      avg /= 100.f * 100.f;
      float target = avg;
      float old = r_exposure.getFloat();
      */

      if (!r_disablepost.getBool()) engine->viewport->unbind(_);

      device->setDepthState(BaseDevice::Always);
      device->setCullState(BaseDevice::None);

      if (!r_disablepost.getBool() && bloomEnabled) {
        {
          profiler.fun("Post process: Bloom");
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
                                  ? engine->viewport->get(1)
                                  : engine->pingpongTexture[!horizontal]
                                        .get()});
                });
            horizontal = !horizontal;
            if (firstIteration) firstIteration = false;
            engine->getDevice()->unbindFramebuffer(framebuffer);
            profiler.end();
          }
        }
      }

      device->viewport(0, 0, engine->windowResolution.x,
                       engine->windowResolution.y);

      if (!r_disablepost.getBool()) {
        profiler.fun("Post process: Draw");
        engine->renderFullscreenQuad(
            engine->viewport->get(0), NULL, [this](BaseProgram* p) {
              if (r_bloom.getBool())
                p->setParameter(
                    r_samples.getInt() ? "texture1ms" : "texture1", DtSampler,
                    BaseProgram::Parameter{
                        .texture.slot = 1,
                        .texture.texture = engine->pingpongTexture[1].get()});
              p->setParameter(
                  r_samples.getInt() ? "texture2ms" : "texture2", DtSampler,
                  BaseProgram::Parameter{
                      .texture.slot = 1,
                      .texture.texture = engine->guiViewport->get()});
              p->setParameter(
                  "bloom", DtInt,
                  BaseProgram::Parameter{.integer = r_bloom.getBool()});
              p->setParameter(
                  "target_res", DtVec2,
                  BaseProgram::Parameter{.vec2 = engine->targetResolution});
              p->setParameter(
                  "window_res", DtVec2,
                  BaseProgram::Parameter{.vec2 = engine->windowResolution});
              p->setParameter("forced_aspect", DtFloat,
                              BaseProgram::Parameter{
                                  .number = (float)engine->forcedAspect});
              p->setParameter(
                  "exposure", DtFloat,
                  BaseProgram::Parameter{.number = r_exposure.getFloat()});
              p->setParameter(
                  "samples", DtInt,
                  BaseProgram::Parameter{.integer = r_samples.getInt()});
            });
        profiler.end();
      }
    } catch (std::exception& e) {
      std::scoped_lock lock(engine->context->getMutex());
      Log::printf(LOG_ERROR, "Error in render: %s", e.what());
    }

    engine->device->stopImGui();

    engine->imguiLock.unlock();

    engine->afterGuiRenderStepped.fire();

    profiler.fun("Swap buffers");

    engine->context->swapBuffers();

    profiler.end();

    profiler.end();

    return Stepped;
  }
};

Engine::Engine(World* world, void* hwnd) {
  this->world = world;
  fullscreenSamples = r_samples.getInt();
  maxFbScale = 1.0;
  forcedAspect = 0.0;

  CVar* r_api = Settings::singleton()->getCvar("r_api");
  ApiFactory::ApiReg regs =
      ApiFactory::singleton()->getFunctions(r_api->getValue().c_str());

  context.reset(regs.createContext(hwnd));
  std::scoped_lock lock(context->getMutex());
  device.reset(regs.createDevice(context.get()));

  device->engine = this;
  textureCache.reset(new TextureCache(device.get()));
  materialCache.reset(new MaterialCache(device.get()));
  meshCache.reset(new MeshCache(this));
  videoRenderer.reset(new VideoRenderer(this));

  clearColor = glm::vec3(0.3, 0.3, 0.3);

  ViewportGfxSettings settings;
  settings.resolution = glm::ivec2(1, 1);
  settings.msaaSamples = fullscreenSamples;
  settings.numColorBuffers = 2;  // color, bloom
  viewport.reset(new Viewport(this, settings));

  settings.resolution = glm::ivec2(1, 1);
  settings.msaaSamples = fullscreenSamples;
  settings.numColorBuffers = 1;
  guiViewport.reset(new Viewport(this, settings));

  fullscreenMaterial =
      materialCache->getOrLoad("PostProcess").value_or(nullptr);
  if (fullscreenMaterial) {
    initializeBuffers(glm::vec2(1.0, 1.0), false);

  } else {
    throw std::runtime_error("Could not load PostProcess material!!!");
  }

  currentViewport = NULL;

  unsigned int tx[] = {0xfffffff};
  whiteTexture = device->createTexture();
  whiteTexture->upload2d(1, 1, DtUnsignedByte, BaseTexture::RGB, tx);

  renderJob = world->getScheduler()->addJob(new RenderJob(this));
  world->stepped.listen([this] { stepped(); });
  isInitialized = false;
}

void Engine::renderFullscreenQuad(
    BaseTexture* texture, Material* material,
    std::function<void(BaseProgram*)> setParameters) {
  if (material == 0) material = fullscreenMaterial.get();
  BaseProgram* fullscreenProgram = material->prepareDevice(device.get(), 0);
  if (fullscreenProgram) {
    if (texture)
      fullscreenProgram->setParameter(
          texture->isMultisampled() ? "texture0ms" : "texture0", DtSampler,
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
  // set up buffers for post processing/hdr
  if (!reset) {
    fullscreenBuffer = device->createBuffer();
    fullScreenArrayPointers = device->createArrayPointers();
    fullScreenArrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DataType::DtVec2, 0, 3, 0, 0, fullscreenBuffer.get()));
    fullscreenBuffer->upload(BaseBuffer::Array, BaseBuffer::StaticDraw,
                             sizeof(float) * 6,
                             (float[]){0.0, 0.0, 2.0, 0.0, 0.0, 2.0});
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
  } catch (std::exception& e) {
    Log::printf(LOG_ERROR, "Creating post fb: %s", e.what());
  }
}

void Engine::stepped() {}

static CVar r_resource_menu("r_resource_menu", "0");

void Engine::render() {
  imguiLock.lock();
  device->startImGui();

  getRenderJob()->getProfiler().fun("Render");

  if (r_resource_menu.getBool())
    getWorld()->getGame()->getResourceManager()->imgui(this);

  ngui->render();

  renderStepped.fire();

  getRenderJob()->getProfiler().fun("Render entities");
  for (int i = 0; i < entities.size(); i++) {
    Entity* ent = entities[i].get();
    try {
      ent->render(device.get());
    } catch (std::exception& error) {
      Log::printf(LOG_ERROR, "Error rendering entity %i", i);
    }
  }
  getRenderJob()->getProfiler().end();

  if (currentViewport) {
    finishViewport(NULL);
  }

  const char* passName[] = {
      "Opaque",
      "Transparent",
      "HUD",
  };

  for (int i = 0; i < RenderPass::_Max; i++) {
    device->dbgPushGroup(passName[i]);
    getRenderJob()->getProfiler().fun(passName[i]);
    void* _;
    switch ((RenderPass::Pass)i) {
      case RenderPass::HUD: {
        _ = setViewport(guiViewport.get());
        getDevice()->clearDepth();
        getDevice()->clear(0.f, 0.f, 0.f, 0.f);
        getDevice()->setDepthState(BaseDevice::Always);
        getDevice()->setBlendStateSeperate(
            BaseDevice::SrcAlpha, BaseDevice::OneMinusSrcAlpha, BaseDevice::One,
            BaseDevice::OneMinusSrcAlpha);
      } break;
      default:
        break;
    }
    passes[i].render(this);
    getRenderJob()->getProfiler().end();
    device->dbgPopGroup();

    switch ((RenderPass::Pass)i) {
      case RenderPass::Opaque:
        afterOpaqueNTransparentRendered.fire();
        break;
      case RenderPass::HUD:
        finishViewport(_);
        break;
      default:
        break;
    }
  }

  device->setDepthState(BaseDevice::Disabled);
  device->setCullState(BaseDevice::None);
  videoRenderer->render();

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

  getRenderJob()->getProfiler().end();
}

void Engine::initialize() {
  ngui = std::unique_ptr<gui::NGuiManager>(new gui::NGuiManager(this));
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

void* Engine::setViewport(Viewport* viewport) {
  void* vp = currentViewport;
  vpRef = viewport->bind();
  viewport->applyRenderState();
  currentViewport = viewport;
  return vp;
}

void Engine::finishViewport(void* _) {
  currentViewport->unbind(vpRef);
  Viewport* oldVp = (Viewport*)_;
  if (oldVp) {
    oldVp->unbind(vpRef);
  }
  currentViewport = oldVp;
  if (currentViewport) {
    currentViewport->applyRenderState();
  } else {
    if (r_disablepost.getBool()) {
    } else {
      viewport->applyRenderState();
    }
  }
}
}  // namespace rdm::gfx
