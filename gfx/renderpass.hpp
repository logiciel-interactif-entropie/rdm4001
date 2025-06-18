#pragma once
#include <vector>

#include "rendercommand.hpp"
namespace rdm::gfx {
class RenderPass {
  std::vector<RenderList> lists;

 public:
  enum Pass {
    Opaque,
    Transparent,
    HUD,
    _Max,
  };

  void add(RenderList list);
  void render(gfx::Engine* engine);
};
};  // namespace rdm::gfx
