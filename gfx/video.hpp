#pragma once
#ifdef RDM4001_FEATURE_MPV
#include <mpv/client.h>
#include <mpv/render_gl.h>
#endif

#include "base_device.hpp"
#include "base_types.hpp"
#include "viewport.hpp"

namespace rdm::gfx {
class Engine;

class VideoRenderer {
  // std::unique_ptr<BaseTexture> videoTexture;
  Viewport videoViewport;

#ifdef RDM4001_FEATURE_MPV
  mpv_handle* handle;
  mpv_render_context* renderer;
#endif

  Engine* engine;
  bool nextFrame;
  bool newMpvEvents;
  bool redraw;

 public:
  VideoRenderer(Engine* engine);
  ~VideoRenderer();

  void play(const char* path);
  void render();

  glm::ivec2 getVideoSize();

  gfx::BaseTexture* getTexture() {
    return videoViewport.get();
    redraw = true;
  }

  enum Status { Playing, Finished, Paused };

  Status getStatus() { return currentStatus; };

  void setNextFrameFlag() { nextFrame = true; };
  void setNewMpvEventsFlag() { newMpvEvents = true; }

 private:
  Status currentStatus;
};
}  // namespace rdm::gfx
