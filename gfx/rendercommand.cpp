#include "rendercommand.hpp"

#include <format>
#include <glm/ext/matrix_transform.hpp>

#include "engine.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/rendercommand.hpp"
namespace rdm::gfx {
RenderCommand::RenderCommand(gfx::BaseDevice::DrawType type,
                             gfx::BaseBuffer* elements, size_t count,
                             gfx::BaseArrayPointers* pointers,
                             gfx::BaseProgram* program, void* first) {
  this->type = type;
  this->pointers = pointers;
  this->program = program;
  this->elements = elements;
  this->count = count;
  this->first = first;
  this->user = NULL;
  for (int i = 0; i < NR_MAX_TEXTURES; i++) texture[i] = NULL;
}

DirtyFields RenderCommand::render(gfx::Engine* engine) {
  DirtyFields df;
  if (program) {
    program->bind();
    df.program = true;
  }
  if (pointers) {
    pointers->bind();
    df.pointers = true;
  }
  engine->getDevice()->draw(elements, DtUnsignedInt, this->type, count, first);
  return df;
}

RenderList::RenderList(gfx::BaseProgram* program,
                       gfx::BaseArrayPointers* pointers,
                       RenderListSettings settings) {
  this->program = program;
  this->pointers = pointers;
  this->settings = settings;
}

RenderCommand* RenderList::add(RenderCommand& command) {
  commands.push_back(command);
  return &commands.back();
}

void RenderList::render(gfx::Engine* engine) {
  engine->getDevice()->setCullState(settings.cull);
  engine->getDevice()->setDepthState(settings.state);

  if (program) program->bind();
  if (pointers) pointers->bind();
  BaseTexture* oldTextures[NR_MAX_TEXTURES] = {0};

  memset(oldTextures, 0, sizeof(oldTextures));
  glm::mat4 lastModel = glm::identity<glm::mat4>();
  glm::vec3 lastColor = glm::vec3(-1.f);
  for (int i = 0; i < commands.size(); i++) {
    RenderCommand command = commands[i];
    if (program) {
      bool needsRebind = false;
      for (int j = 0; j < NR_MAX_TEXTURES; j++) {
        BaseTexture* texture = command.getTexture(j);
        if (texture && oldTextures[j] != texture) {
          program->setParameter(
              std::format("texture{}", j), DtSampler,
              {.texture.slot = j, .texture.texture = texture});
          oldTextures[j] = texture;
          needsRebind = true;
        }
      }
      if (command.getModel()) {
        glm::mat4 model = command.getModel().value();
        if (lastModel != model) {
          program->setParameter("model", DtMat4, {.matrix4x4 = model});
          lastModel = model;
          needsRebind = true;
        }
      }
      if (command.getColor()) {
        glm::vec3 color = command.getColor().value();
        if (lastColor != color) {
          program->setParameter("color", DtVec3, {.vec3 = color});
          lastColor = color;
          needsRebind = true;
        }
      }
      if (command.getOffset()) {
        program->setParameter("offset", DtVec2,
                              {.vec2 = command.getOffset().value()});
        needsRebind = true;
      }
      if (command.getScale()) {
        program->setParameter("scale", DtVec2,
                              {.vec2 = command.getScale().value()});
        needsRebind = true;
      }
      if (needsRebind) program->bind();
    }
    DirtyFields df = command.render(engine);
    if (df.program && program) program->bind();
    if (df.pointers && pointers) pointers->bind();
  }
}
};  // namespace rdm::gfx
