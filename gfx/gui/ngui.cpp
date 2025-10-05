#include "ngui.hpp"

#include <stdarg.h>
#include <stdlib.h>

#include <list>
#include <world.hpp>

#include "font.hpp"
#include "game.hpp"
#include "gfx/engine.hpp"
#include "input.hpp"
#include "localization.hpp"
#include "ngui_elements.hpp"
#include "ngui_window.hpp"
#include "pak_file.hpp"
#include "settings.hpp"

namespace rdm::gfx::gui {
RDM_REFLECTION_BEGIN_DESCRIBED(NGuiManager);
RDM_REFLECTION_PRECACHE_FUNC(NGuiManager, [](ResourceManager* rmgr) {
  common::FileSystem::singleton()->addApi(
      new pak::PakFile("engine/gui/ngui.pak"), "gui_pak");
  rmgr->load<resource::Texture>("gui_pak://close_button.png");
  rmgr->load<resource::Texture>("gui_pak://title_bar.png");
  rmgr->load<resource::Texture>("gui_pak://corner_r.png");
  rmgr->load<resource::Texture>("gui_pak://corner_l.png");
  rmgr->load<resource::Texture>("gui_pak://hbar.png");
  rmgr->load<resource::Texture>("gui_pak://vlbar.png");
  rmgr->load<resource::Texture>("gui_pak://vrbar.png");
});
RDM_REFLECTION_END_DESCRIBED();

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

  textInput = NULL;
}

void NGuiManager::render() {
  engine->getRenderJob()->getProfiler().fun("ngui compose");
  std::string& tin = Input::singleton()->getEditedText();
  if (textInput) {
    if (tin.size() >= textInputBufSize - 1) {
      tin = tin.substr(0, textInputBufSize - 1);
    }
    memset(textInput, 0, textInputBufSize);
    strncpy(textInput, tin.data(), textInputBufSize);
  }

  int texNum = 0;
  std::vector<NGuiRenderer*> renderers;
  for (auto [name, gui] : guis) {
    NGuiRenderer* renderer = new NGuiRenderer(this, engine, texNum);
    gui->render(renderer);
    texNum += renderer->texNum;
    if (renderer->submittedCommands)
      engine->pass(RenderPass::HUD).add(renderer->getList());
    renderers.push_back(renderer);
  }
  engine->pass(RenderPass::HUD)
      .sort([](RenderList const& a, RenderList const& b) {
        NGuiRenderer* _a = (NGuiRenderer*)a.getUser();
        NGuiRenderer* _b = (NGuiRenderer*)b.getUser();

        if (_a->getZIndex() == _b->getZIndex()) return _a < _b;
        return _a->getZIndex() > _b->getZIndex();
      });
  for (auto renderer : renderers) {
    delete renderer;
  }
  engine->getRenderJob()->getProfiler().end();
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

    if (ctm->maxWidth == 0) {
      OutFontTexture out = FontRender::render(font, text);
      if (out.data == NULL) throw std::runtime_error("out.data == NULL");
      ctm->height = out.h;
      ctm->width = out.w;

      ctm->texture->upload2d(out.w, out.h, DtUnsignedByte, BaseTexture::RGBA,
                             out.data);
    } else {
      int acMaxWidth = maxWidth;
      if (ctm->maxWidth == -1) acMaxWidth = 0;
      OutFontTexture out = FontRender::renderWrapped(font, text, acMaxWidth);
      if (out.data == NULL) throw std::runtime_error("out.data == NULL");
      ctm->height = out.h;
      ctm->width = out.w;

      ctm->texture->upload2d(out.w, out.h, DtUnsignedByte, BaseTexture::RGBA,
                             out.data);
    }
  }

  d.texture = ctm->texture.get();
  d.height = ctm->height;
  d.width = ctm->width;

  return d;
}

static RenderListSettings settings(BaseDevice::None, BaseDevice::Always);

NGuiRenderer::NGuiRenderer(NGuiManager* manager, gfx::Engine* engine,
                           int texNum)
    : list(manager->getImageMaterial()->prepareDevice(engine->getDevice(), 0),
           manager->getSArrayPointers(), settings) {
  this->manager = manager;
  this->engine = engine;
  color = glm::vec3(1);

  zIndex = 1337;
  submittedCommands = 0;
  list.setUser(this);

  this->texNum = texNum;
}

std::pair<int, int> NGuiRenderer::text(glm::ivec2 position, Font* font,
                                       int maxWidth, const char* text, ...) {
  va_list ap;
  va_start(ap, text);
  char buf[2048];
  vsnprintf(buf, 2048, text, ap);

  if (strlen(buf) == 0) strcpy(buf, " ");

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

  target = glm::round(target);

  RenderCommand command(BaseDevice::Triangles, manager->getSElementBuf(), 6,
                        NULL, NULL, NULL);
  command.setOffset(target);
  command.setScale(glm::vec2(d.width, d.height));
  command.setColor(color);
  command.setTexture(0, d.texture);
  lastCommand = list.add(command);

  submittedCommands++;

  va_end(ap);
  return out;
}

void NGuiRenderer::image(gfx::BaseTexture* image, glm::vec2 position,
                         glm::vec2 size) {
  if (!image) image = engine->getWhiteTexture();

  glm::vec2 target = glm::vec2(position.x, position.y);
  glm::vec2 window = engine->getTargetResolution();
  if (target.x < 0) {
    target.x = (window.x + target.x) - size.x;
  }
  if (target.y < 0) {
    target.y = (window.y + target.y) - size.y;
  }

  target = glm::round(target);
  size = glm::round(size);

  RenderCommand command(BaseDevice::Triangles, manager->getSElementBuf(), 6,
                        NULL, NULL, NULL);
  command.setOffset(target);
  command.setScale(size);
  command.setColor(color);
  command.setTexture(0, image);
  lastCommand = list.add(command);

  submittedCommands++;
}

int NGuiRenderer::mouseDownZone(glm::vec2 pos, glm::vec2 size) {
  float scale = Settings::singleton()->getCvar("r_scale")->getFloat();

  glm::vec2 mp = Input::singleton()->getMousePosition();
  glm::vec2 res = getEngine()->getTargetResolution();

  bool oob = false;

  pos = glm::round(pos);
  size = glm::round(size);

  mp.y = res.y - mp.y;
  if (mp.x < pos.x) oob = true;
  if (mp.x > pos.x + size.x) oob = true;
  if (mp.y < pos.y) oob = true;
  if (mp.y > pos.y + size.y) oob = true;
  for (int i = 0; i < 3; i++) {
    if (Input::singleton()->isMouseButtonDown(i)) return oob ? -(i + 1) : i;
  }
  return oob ? -1 : 0;
}

void NGuiManager::setCurrentText(char* t, size_t l) {
  textInput = t;
  textInputBufSize = l;
  if (textInput) {
    Input::singleton()->startEditingText();
    Input::singleton()->getEditedText() = textInput;
  } else {
    Input::singleton()->stopEditingText();
  }
}

NGui::NGui(NGuiManager* gui, gfx::Engine* engine) {
  this->gui = gui;
  this->engine = engine;
}

Game* NGui::getGame() { return engine->getWorld()->getGame(); }

gfx::Engine* NGui::getEngine() { return engine; }

ResourceManager* NGui::getResourceManager() {
  return getGame()->getResourceManager();
}

#ifndef NDEBUG
static CVar cl_showfps("cl_showfps", "1", CVARF_SAVE | CVARF_GLOBAL);
#else
static CVar cl_showfps("cl_showfps", "0", CVARF_SAVE | CVARF_GLOBAL);
#endif

class FPSDisplay : public NGui {
  Font* font;
  std::list<float> frameTimes;
  std::vector<const char*> shownBlocks;
  int blockYoff = 0;

  void renderBlock(NGuiRenderer* renderer, int lvl, Profiler::Block* block) {
    if (!block->name) return;

    int id = 0;
    for (int i = 0; i < strlen(block->name); i++) {
      id += block->name[i];
    }

    glm::vec3 color = glm::vec3((id & 0xf) / 15.f, ((id & 0xf0) >> 4) / 15.f,
                                ((id & 0xf00) >> 8) / 15.f);

    glm::vec2 res = getEngine()->getTargetResolution();

    const float size =
        res.x / getEngine()->getRenderJob()->getStats().deltaTime;

    float b = block->begin.count() * size;
    float t = block->time.count() * size;
    if (cl_showfps.getInt() == 4)
      t = getEngine()->getRenderJob()->getProfiler().getBlockAvg(block->name);

    renderer->setColor(color);
    renderer->image(getEngine()->getWhiteTexture(),
                    glm::vec2(b, 100.f + lvl * 8), glm::vec2(t, 8.f));

    if (std::find(shownBlocks.begin(), shownBlocks.end(), block->name) ==
        shownBlocks.end()) {
      shownBlocks.push_back(block->name);

      renderer->setColor(glm::vec3(1) - color);
      blockYoff -=
          renderer
              ->text(glm::ivec2(b, 100.f + lvl * 8), font, 0, "%s", block->name)
              .second;
    }

    for (Profiler::Block& blockChild : block->children) {
      renderBlock(renderer, lvl + 1, &blockChild);
    }
  }

 public:
  FPSDisplay(NGuiManager* gui, gfx::Engine* engine) : NGui(gui, engine) {
    font = gui->getFontCache()->get("gui_pak://serif.ttf", 8);
  }

  virtual void render(NGuiRenderer* renderer) {
    if (!cl_showfps.getBool()) return;

    renderer->setZIndex(INT32_MAX);

    glm::vec2 res = getEngine()->getTargetResolution();

    const int baseline = -1;
    const float sampleWidth = res.x / 100.f / 4.f;
    const float sampleSize =
        res.y / getEngine()->getRenderJob()->getFrameRate() / 4.f;

    renderer->setColor(
        glm::mix(glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0),
                 renderer->getEngine()->getRenderJob()->getFrameRate() /
                     renderer->getEngine()
                         ->getRenderJob()
                         ->getStats()
                         .getAvgDeltaTime()));
    renderer->text(glm::ivec2(0, baseline), font, 0, "FPS %f",
                   1.0 / renderer->getEngine()
                             ->getRenderJob()
                             ->getStats()
                             .getAvgDeltaTime());
    renderer->text(glm::ivec2(-1, baseline), font, -1,
                   Lc(RDM_COPYRIGHT_STRING,
                      "© logiciel interactif entropie 2024-2026\nRDM4001 is "
                      "licensed under the GNU AGPLv3"));

    if (cl_showfps.getInt() == 2) {
      frameTimes.push_back(getEngine()->getRenderJob()->getStats().deltaTime);
      if (frameTimes.size() >= 100) frameTimes.pop_front();
      int xoff = 0;
      for (float sample : frameTimes) {
        renderer->setColor(glm::mix(
            glm::vec3(0.0, 1.0, 0.0), glm::vec3(1.0, 0.0, 0.0),
            sample / renderer->getEngine()->getRenderJob()->getFrameRate()));
        renderer->image(getEngine()->getWhiteTexture(), glm::vec2(xoff, 0),
                        glm::vec2(sampleWidth, sample * sampleSize));
        xoff += sampleWidth;
      }

      renderer->setColor(glm::vec3(0.2f));
      renderer->text(glm::ivec2(0, getEngine()->getRenderJob()->getFrameRate() *
                                       sampleSize),
                     font, 0, "BELOW OK (%0.2f%% BUDGET)",
                     (getEngine()->getRenderJob()->getStats().deltaTime /
                      getEngine()->getRenderJob()->getFrameRate()) *
                         100.f);
      renderer->image(getEngine()->getWhiteTexture(),
                      glm::vec2(0, getEngine()->getRenderJob()->getFrameRate() *
                                       sampleSize),
                      glm::vec2(100, 1));
    } else if (cl_showfps.getInt() == 3 || cl_showfps.getInt() == 4) {
      blockYoff = res.y / 2.f;
      shownBlocks.clear();
      renderBlock(renderer, 0,
                  getEngine()->getRenderJob()->getProfiler().getLastFrame());
    }
  }
};

NGUI_INSTANTIATOR(FPSDisplay);

#ifdef MAKE_COOL_WINDOW
class CoolWindow : public NGuiWindow {
 public:
  CoolWindow(NGuiManager* gui, gfx::Engine* engine) : NGuiWindow(gui, engine) {
    setTitle("Cool window");

    TextLabel* label0 = new TextLabel(gui);
    label0->setText("Hello world");
    addElement(label0);

    Button* button0 = new Button(gui);
    button0->setText("Okay");
    button0->setPressed(std::bind(&CoolWindow::pressed, this));
    addElement(button0);

    NGuiPanel* panel0 = new NGuiPanel(gui);
    panel0->setLayout(new NGuiHorizontalLayout());
    addElement(panel0);

    TextLabel* label1 = new TextLabel(gui);
    label1->setText("Haha ");
    panel0->addElement(label1);

    TextLabel* label2 = new TextLabel(gui);
    label2->setText("Friend ");
    panel0->addElement(label2);

    open();
  }

  void pressed() { Log::printf(LOG_DEBUG, "HI"); }
};
NGUI_INSTANTIATOR(CoolWindow);
#endif
}  // namespace rdm::gfx::gui
