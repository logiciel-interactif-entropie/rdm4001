#include "ngui.hpp"

#include <stdarg.h>
#include <stdlib.h>

#include "font.hpp"
#include "gfx/engine.hpp"

namespace rdm::gfx::gui {
NGuiSingleton::NGuiSingleton() {}

static NGuiSingleton* _singleton = NULL;
NGuiSingleton* NGuiSingleton::singleton() {
  if (!_singleton) _singleton = new NGuiSingleton();
  return _singleton;
}

NGuiManager::NGuiManager(gfx::Engine* engine) {
  fontCache.reset(new FontCache());

  this->engine = engine;
  for (auto [name, ctor] : NGuiSingleton::singleton()->guiCtor) {
    guis[name] = ctor(this, engine);
  }

  gfx::MaterialCache* cache = engine->getMaterialCache();
  image = cache->getOrLoad("GuiImage").value();

  float squareVtx[] = {0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0};
  squareArrayBuffer = engine->getDevice()->createBuffer();
  squareArrayBuffer->upload(gfx::BaseBuffer::Array, gfx::BaseBuffer::StaticDraw,
                            sizeof(squareVtx), squareVtx);

  unsigned int squareIndex[] = {0, 1, 2, 2, 1, 3};
  squareElementBuffer = engine->getDevice()->createBuffer();
  squareElementBuffer->upload(gfx::BaseBuffer::Element,
                              gfx::BaseBuffer::StaticDraw, sizeof(squareIndex),
                              squareIndex);

  squareArrayPointers = engine->getDevice()->createArrayPointers();
  squareArrayPointers->addAttrib(BaseArrayPointers::Attrib(
      DtFloat, 0, 2, sizeof(float) * 2, 0, squareArrayBuffer.get()));
  squareArrayPointers->upload();
}

void NGuiManager::render() {
  NGuiRenderer renderer(this, engine);
  for (auto [name, gui] : guis) {
    gui->render(&renderer);
  }
  engine->pass(RenderPass::HUD).add(renderer.getList());
}

NGuiManager::TexOutData NGuiManager::getTextTexture(int tn, Font* font,
                                                    int maxWidth,
                                                    const char* text) {
  CacheTextMember* ctm = NULL;
  TexOutData d;
  if (cacheMember.find(tn) == cacheMember.end()) {
    cacheMember[tn] = CacheTextMember();
    ctm = &cacheMember[tn];
    ctm->texture = engine->getDevice()->createTexture();
    ctm->font = font;
    ctm->text = "";
  } else {
    ctm = &cacheMember[tn];
  }

  if (ctm->text != text || ctm->font != font || ctm->maxWidth != maxWidth) {
    ctm->text = text;
    ctm->font = font;
    ctm->maxWidth = maxWidth;

    OutFontTexture out = FontRender::render(font, text);
    if (out.data == NULL) throw std::runtime_error("out.data == NULL");
    ctm->height = out.h;
    ctm->width = out.w;

    ctm->texture->upload2d(out.w, out.h, DtUnsignedByte, BaseTexture::RGBA,
                           out.data);
  }

  d.texture = ctm->texture.get();
  d.height = ctm->height;
  d.width = ctm->width;

  return d;
}

static RenderListSettings settings(BaseDevice::None, BaseDevice::Always);

NGuiRenderer::NGuiRenderer(NGuiManager* manager, gfx::Engine* engine)
    : list(manager->getImageMaterial()->prepareDevice(engine->getDevice(), 0),
           manager->getSArrayPointers(), settings) {
  // list.getProgram()->setParameter("uiProjectionMatrix", DtMat4,
  //                                 {.matrix4x4 = manager->getUiMatrix()});

  this->manager = manager;
  this->engine = engine;
  color = glm::vec3(1);

  texNum = 0;
}

std::pair<int, int> NGuiRenderer::text(glm::ivec2 position, Font* font,
                                       int maxWidth, const char* text, ...) {
  va_list ap;
  va_start(ap, text);
  char buf[2048];
  vsnprintf(buf, 2048, text, ap);

  std::pair<int, int> out;
  NGuiManager::TexOutData d =
      manager->getTextTexture(texNum, font, maxWidth, buf);
  out.first = d.width;
  out.second = d.height;
  texNum++;

  glm::vec2 target = glm::vec2(position.x, position.y);
  glm::vec2 window = engine->getTargetResolution();
  if (target.x < 0) {
    target.x = (window.x + target.x) - d.width;
  }
  if (target.y < 0) {
    target.y = (window.y + target.y) - d.height;
  }

  RenderCommand command(BaseDevice::Triangles, manager->getSElementBuf(), 6,
                        NULL, NULL, NULL);
  command.setOffset(target);
  command.setScale(glm::vec2(d.width, d.height));
  command.setColor(color);
  command.setTexture(0, d.texture);
  list.add(command);

  va_end(ap);
  return out;
}

NGui::NGui(NGuiManager* gui, gfx::Engine* engine) {
  this->gui = gui;
  this->engine = engine;
}

class FPSDisplay : public NGui {
  Font* font;

 public:
  FPSDisplay(NGuiManager* gui, gfx::Engine* engine) : NGui(gui, engine) {
    font = gui->getFontCache()->get("dat3/serif.ttf", 19);
  }

  virtual void render(NGuiRenderer* renderer) {
    renderer->setColor(
        glm::mix(glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),

                 renderer->getEngine()->getRenderJob()->getFrameRate() /
                     renderer->getEngine()
                         ->getRenderJob()
                         ->getStats()
                         .getAvgDeltaTime()));
    renderer->text(glm::ivec2(0, -1), font, 0, "FPS %f",
                   1.0 / renderer->getEngine()
                             ->getRenderJob()
                             ->getStats()
                             .getAvgDeltaTime());
    renderer->text(glm::ivec2(-1, -1), font, 0,
                   "© logiciel interactif entropie 2024-2026, RDM4001 is "
                   "licensed under the GNU GPLv3");
  }
};

NGUI_INSTANTIATOR(FPSDisplay);
}  // namespace rdm::gfx::gui
