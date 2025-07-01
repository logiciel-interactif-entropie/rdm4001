#include "video.hpp"

#include <mpv/client.h>
#include <mpv/render.h>

#include "SDL_video.h"
#include "engine.hpp"
#include "gl_types.hpp"
#include "gui/ngui.hpp"
#include "gui/ngui_window.hpp"
#include "viewport.hpp"
#include "world.hpp"
namespace rdm::gfx {
#ifndef RDM4001_FEATURE_GLMODERN
#error Need GLModern support to use VideoRenderer
#endif

static void mpv_render_event(void *ctx) {
  ((VideoRenderer *)ctx)->setNextFrameFlag();
}

static void mpv_event(void *ctx) {
  ((VideoRenderer *)ctx)->setNewMpvEventsFlag();
}

static void *get_proc_address_mpv(void *fn_ctx, const char *name) {
  return SDL_GL_GetProcAddress(name);
}

class VideoRendererWindow : public gui::NGuiWindow {
 public:
  VideoRendererWindow(gui::NGuiManager *manager, Engine *engine)
      : NGuiWindow(manager, engine) {
    open();
  }

  virtual void show(Render *render) {
    render->image(glm::vec2(getEngine()->getVideoRenderer()->getVideoSize()),
                  getEngine()->getVideoRenderer()->getTexture());
    setSize(glm::vec2(getEngine()->getVideoRenderer()->getVideoSize()) +
            glm::vec2(30, 15));
  }
};

NGUI_INSTANTIATOR(VideoRendererWindow);

VideoRenderer::VideoRenderer(gfx::Engine *engine) : videoViewport(engine) {
  this->engine = engine;
  handle = mpv_create();
  if (!handle) throw std::runtime_error("Could not create MPV context");
  // mpv_set_option_string(handle, "keepaspect", "no");
  mpv_set_option_string(handle, "loop-file", "inf");
  mpv_set_option_string(handle, "vo", "libmpv");
  if (mpv_initialize(handle) < 0)
    throw std::runtime_error("Could not initialize MPV context");
  ViewportGfxSettings settings = videoViewport.getSettings();
  settings.resolution = glm::ivec2(800, 600);
  videoViewport.updateSettings(settings);
  mpv_opengl_init_params glInitParams[1] = {get_proc_address_mpv, NULL};
  int _ = 1;
  mpv_render_param param[]{{MPV_RENDER_PARAM_API_TYPE,
                            const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
                           {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
                           {MPV_RENDER_PARAM_ADVANCED_CONTROL, &_},
                           {MPV_RENDER_PARAM_INVALID, NULL}};
  if (mpv_render_context_create(&renderer, handle, param) < 0)
    throw std::runtime_error("Could not initialize MPV GL context");
  mpv_set_wakeup_callback(handle, mpv_event, this);
  mpv_render_context_set_update_callback(renderer, mpv_event, this);
}

VideoRenderer::~VideoRenderer() {
  mpv_render_context_free(renderer);
  mpv_destroy(handle);
}

void VideoRenderer::play(const char *path) {
  const char *cmd[] = {"loadfile", path, NULL};
  mpv_command_async(handle, 0, cmd);
  currentStatus = Status::Playing;
}

void VideoRenderer::render() {
  if (currentStatus == Status::Playing) {
    engine->getRenderJob()->getProfiler().fun("video renderer");
    if (nextFrame) {
      uint64_t flags = mpv_render_context_update(renderer);
      if (flags & MPV_RENDER_UPDATE_FRAME) redraw = true;
      nextFrame = false;
    }

    if (newMpvEvents) {
      while (true) {
        struct mpv_event *mpEvent = mpv_wait_event(handle, 0);
        if (mpEvent->event_id == MPV_EVENT_NONE) break;
        if (mpEvent->event_id == MPV_EVENT_LOG_MESSAGE) {
          mpv_event_log_message *msg = (mpv_event_log_message *)mpEvent->data;
          Log::printf(LOG_EXTERNAL, "%s", msg->text);
          continue;
        }
      }
      newMpvEvents = false;
    }

    if (redraw) {
      void *_ = engine->setViewport(&videoViewport);
      engine->getDevice()->clearDepth();
      engine->getDevice()->clear(0.f, 0.f, 0.f, 1.0f);
      mpv_opengl_fbo fboParam = {
          .fbo = (int)dynamic_cast<gl::GLFrameBuffer *>(videoViewport.getFb())
                     ->getId(),
          .w = videoViewport.getSettings().resolution.x,
          .h = videoViewport.getSettings().resolution.y,
      };
      int flip = 1;
      mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &fboParam},
                                   {MPV_RENDER_PARAM_FLIP_Y, &flip},
                                   {MPV_RENDER_PARAM_INVALID, NULL}};
      mpv_render_context_render(renderer, params);
      engine->finishViewport(_);
    }
    engine->getRenderJob()->getProfiler().end();
  }
}  // namespace rdm::gfx

glm::ivec2 VideoRenderer::getVideoSize() {
  return videoViewport.getSettings().resolution;
}
}  // namespace rdm::gfx
