#pragma once
#include <algorithm>
#include <optional>
#include <vector>

#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
namespace rdm::gfx {
class Engine;

struct DirtyFields {
  bool program;
  bool pointers;
};

#define NR_MAX_TEXTURES 4

class RenderCommand {
  gfx::BaseProgram* program;
  gfx::BaseArrayPointers* pointers;
  gfx::BaseBuffer* elements;
  gfx::BaseDevice::DrawType type;
  gfx::BaseTexture* texture[NR_MAX_TEXTURES];
  std::optional<glm::mat4> model;
  std::optional<glm::vec2> scale;
  std::optional<glm::vec2> offset;
  std::optional<glm::vec3> color;
  size_t count;
  void* first;
  void* user;

 public:
  RenderCommand(gfx::BaseDevice::DrawType type, gfx::BaseBuffer* elements,
                size_t count, gfx::BaseArrayPointers* pointers = 0,
                gfx::BaseProgram* program = 0, void* first = 0);

  void setTexture(int id, gfx::BaseTexture* texture) {
    this->texture[id] = texture;
  }
  void setModel(std::optional<glm::mat4> model) { this->model = model; }
  void setScale(std::optional<glm::vec2> scale) { this->scale = scale; }
  void setOffset(std::optional<glm::vec2> offset) { this->offset = offset; }
  void setColor(std::optional<glm::vec3> color) { this->color = color; }
  void setUser(void* v) { user = v; };
  gfx::BaseTexture* getTexture(int id) const { return texture[id]; }
  std::optional<glm::mat4> getModel() const { return model; };
  std::optional<glm::vec2> getScale() const { return scale; };
  std::optional<glm::vec2> getOffset() const { return offset; };
  std::optional<glm::vec3> getColor() const { return color; }
  void* getUser() const { return user; }

  DirtyFields render(gfx::Engine* engine);
};

struct RenderListSettings {
  BaseDevice::CullState cull;
  BaseDevice::DepthStencilState state;

  RenderListSettings(BaseDevice::CullState cull = BaseDevice::FrontCCW,
                     BaseDevice::DepthStencilState state = BaseDevice::LEqual) {
    this->cull = cull;
    this->state = state;
  }
};

class RenderList {
  gfx::BaseProgram* program;
  gfx::BaseArrayPointers* pointers;
  void* user;

  std::vector<RenderCommand> commands;
  RenderListSettings settings;

 public:
  RenderList(gfx::BaseProgram* program, gfx::BaseArrayPointers* pointers,
             RenderListSettings settings = RenderListSettings());

  void setUser(void* user) { this->user = user; }
  void* getUser() const { return user; }

  gfx::BaseProgram* getProgram() const { return program; }
  void clear() { commands.clear(); }
  RenderCommand* add(RenderCommand& command);
  void render(gfx::Engine* engine);

  template <typename T>
  void sort(T fun) {
    std::sort(commands.begin(), commands.end(), fun);
  }
};
};  // namespace rdm::gfx
