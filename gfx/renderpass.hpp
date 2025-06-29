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

  size_t numLists() { return lists.size(); }

  template <typename T>
  void sort(T fun) {
    std::sort(lists.begin(), lists.end(), fun);
  }
};
};  // namespace rdm::gfx
