#include "gui.hpp"

#include <format>
#include <numeric>

#include "filesystem.hpp"
#include "fun.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/gui/api.hpp"
#include "gfx/gui/font.hpp"
#include "input.hpp"
#include "logging.hpp"
#include "rapidxml.hpp"
#include "rapidxml_utils.hpp"
#include "script/my_basic.h"
#include "world.hpp"

namespace rdm::gfx::gui {
GuiManager::GuiManager(gfx::Engine* engine) {
  gfx::MaterialCache* cache = engine->getMaterialCache();
  panel = cache->getOrLoad("GuiPanel").value();
  text = cache->getOrLoad("GuiText").value();
  image = cache->getOrLoad("GuiImage").value();

  fontCache.reset(new FontCache());

  this->engine = engine;

  float squareVtx[] = {0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0};
  squareArrayBuffer = engine->getDevice()->createBuffer();
  squareArrayBuffer->upload(gfx::BaseBuffer::Array, gfx::BaseBuffer::StaticDraw,
                            sizeof(squareVtx), squareVtx);

  unsigned char squareIndex[] = {0, 1, 2, 2, 1, 3};
  squareElementBuffer = engine->getDevice()->createBuffer();
  squareElementBuffer->upload(gfx::BaseBuffer::Element,
                              gfx::BaseBuffer::StaticDraw, sizeof(squareIndex),
                              squareIndex);

  squareArrayPointers = engine->getDevice()->createArrayPointers();
  squareArrayPointers->addAttrib(BaseArrayPointers::Attrib(
      DtFloat, 0, 2, sizeof(float) * 2, 0, squareArrayBuffer.get()));
  squareArrayPointers->upload();

  // parseXml("dat3/root.xml");
  // parseXml(std::format("dat3/{}.xml",
  // engine->getWorld()->getName()).c_str());
}

void Component::scriptUpdate(script::Script* script) {
  struct mb_interpreter_t* bas = script->getInterpreter();

  API::registerApi(bas);

  for (auto& var : inVars) {
  }
}

void Component::layoutUpdate(TreeNode* root) {
  for (TreeNode& node : root->children) {
    layoutUpdate(&node);
  }
}

static std::map<std::string, glm::vec3> definedColors = {
    {"white", glm::vec3(1.0)},
    {"gray", glm::vec3(0.5)},
    {"black", glm::vec3(0.0)},
    {"red", glm::vec3(1.0, 0.0, 0.0)},
    {"green", glm::vec3(0.0, 1.0, 0.0)},
    {"pink", glm::vec3(1.0, 0.0, 1.0)},
    {"blue", glm::vec3(0.0, 0.0, 1.0)},
    {"link", glm::vec3(0.2, 0.2, 0.6)},
    {"link_hover", glm::vec3(0.2, 0.2, 1.0)},
    {"link_press", glm::vec3(0.5, 0.2, 1.0)},
    {"textarea", glm::vec3(0.3)},
    {"textarea_edit", glm::vec3(0.1)},
    {"pinkish", glm::vec3(0.84, 0.48, 0.72)}};

bool Component::isVisible() {
  if (parentComponent.empty()) {
    return domRoot.visible;
  } else {
    auto c = manager->getComponentByName(parentComponent);
    if (c) {
      return domRoot.visible && c.value()->isVisible();
    } else {
      return domRoot.visible;
    }
  }
}

void Component::render(GuiManager* manager, gfx::Engine* engine) {
  if (!isVisible()) return;

  glm::vec2 offset = glm::vec2(0);
  glm::ivec2 inc_factor;
  glm::vec2 fbSize = engine->getTargetResolution();
  bool alignRight = false;
  bool alignTop = false;

  if (variablesDirty) {
    variableChanged.fire();
    variablesDirty = false;
  }

  switch (grow) {
    case Horizontal:
      inc_factor = glm::vec2(1, 0);
      break;
    case Vertical:
      inc_factor = glm::vec2(0, 1);
      break;
  }

  switch (anchor) {
    case TopRight:
      inc_factor = -inc_factor;
      alignRight = true;
      alignTop = true;
      offset.y = fbSize.y;
      break;
    case TopLeft:
      inc_factor = -inc_factor;
      alignTop = true;
      offset.y = fbSize.y;
      break;
    case BottomRight:
      alignRight = true;
      break;
    case BottomLeft:
      break;
  }

  int padding = 4;
  gfx::BaseProgram* bp = manager->image->prepareDevice(engine->getDevice(), 0);
  for (auto& index : elementIndex) {
    std::pair<std::string, Element*> element =
        std::make_pair(index, &elements[index]);
    switch (element.second->type) {
      case Element::TextField:
        if (selectedElement == &elements[index]) {
          std::string inText = Input::singleton()->getEditedText();
          if (inText != element.second->value) {
            element.second->value = inText;
            element.second->dirty = true;
          }
          element.second->color = definedColors["textarea_active"];
        } else {
          element.second->color = definedColors["textarea"];
        }
        if (element.second->value.empty()) {
          element.second->value = "...........";
          element.second->dirty = true;
        }
        bp->setParameter("bgcolor", DtVec4, {.vec4 = glm::vec4(0.4)});
      case Element::Label: {
        if (element.second->dirty) {
          OutFontTexture t = FontRender::render(element.second->font,
                                                element.second->value.c_str());
          if (t.data != NULL) {
            element.second->texture->upload2d(t.w, t.h, DtUnsignedByte,
                                              BaseTexture::RGBA, t.data);
            element.second->dirty = false;
            element.second->textureSize = glm::ivec2(t.w, t.h);
          } else {
            throw std::runtime_error("t.data == NULL");
          }
        }
      }
      case Element::Image: {
        if (alignTop)
          offset += inc_factor * (glm::max(element.second->minSize,
                                           element.second->textureSize) +
                                  glm::ivec2(padding));
        BaseTexture* displayTexture = element.second->texture;
        glm::vec3 displayColor = element.second->color;
        glm::vec2 pos = Input::singleton()->getMousePosition();
        glm::vec2 displayOffset = offset;
        if (alignRight)
          displayOffset.x -= 2;
        else
          displayOffset.x += 2;
        if (Math::pointInRect2d(
                glm::vec4(
                    displayOffset.x +
                        (alignRight ? -element.second->textureSize.x + fbSize.x
                                    : 0),
                    fbSize.y - displayOffset.y - element.second->textureSize.y,
                    element.second->textureSize.x,
                    element.second->textureSize.y),
                pos)) {
          if (element.second->link) displayColor = definedColors["link_hover"];
          if (element.second->textureHover)
            displayTexture = element.second->textureHover;
          if (Input::singleton()->isMouseButtonDown(1)) {
            if (element.second->link)
              displayColor = definedColors["link_press"];
            if (element.second->texturePressed)
              displayTexture = element.second->texturePressed;
            if (!element.second->pressed) {
              if (element.second->type == Element::TextField) {
                Input::singleton()->startEditingText(true);
                selectedElement = &elements[index];
                Log::printf(LOG_DEBUG, "Started editing text");
              } else {
                element.second->mouseDown.fire();
              }
              element.second->pressed = true;
            }
          } else {
            element.second->pressed = false;
          }
        } else {
          if (Input::singleton()->isMouseButtonDown(1)) {
            if (selectedElement == &elements[index]) {
              selectedElement = NULL;
            }
          }
          element.second->pressed = false;
        }
        bp->setParameter(
            "texture0", DtSampler,
            BaseProgram::Parameter{.texture.slot = 0,
                                   .texture.texture = displayTexture});
        bp->setParameter(
            "scale", DtVec2,
            BaseProgram::Parameter{.vec2 = element.second->textureSize});
        bp->setParameter("color", DtVec3,
                         BaseProgram::Parameter{.vec3 = displayColor});
        if (element.second->type != Element::TextField) {
          bp->setParameter("bgcolor", DtVec4,
                           {.vec4 = glm::vec4(displayColor, 0.0)});
        }
        if (alignRight) {
          bp->setParameter(
              "offset", DtVec2,
              BaseProgram::Parameter{
                  .vec2 =
                      displayOffset +
                      glm::vec2(-element.second->textureSize.x + fbSize.x, 0)});

        } else {
          bp->setParameter("offset", DtVec2,
                           BaseProgram::Parameter{.vec2 = displayOffset});
        }
        bp->bind();
        if (!alignTop)
          offset += inc_factor * (glm::max(element.second->minSize,
                                           element.second->textureSize) +
                                  glm::ivec2(padding));
        manager->squareArrayPointers->bind();
        engine->getDevice()->draw(manager->squareElementBuffer.get(),
                                  DtUnsignedByte, BaseDevice::Triangles, 6);
      } break;
      default:
        break;
    }
    element.second->dirty = false;
  }
}

static void parseXmlNode(GuiManager* manager, Component* component,
                         TreeNode* parentNode,
                         rapidxml::xml_node<>* parentXmlNode) {
  for (rapidxml::xml_node<>* child = parentXmlNode->first_node(); child;
       child = child->next_sibling()) {
    try {
      TreeNode node;
      node.div = false;
      std::string name = child->name();
      std::string elid;
      if (name == "label") {
        rapidxml::xml_attribute<>* id = child->first_attribute("id");
        elid = id ? id->value()
                  : std::format("label-{}", component->elements.size());

        Component::Element& em = component->elements[elid];
        em.type = Component::Element::Label;
        em.value = child->value();

        rapidxml::xml_attribute<>* fontname = child->first_attribute("font");
        std::string _fontname =
            fontname ? fontname->value() : "dat3/default.ttf";

        rapidxml::xml_attribute<>* ptsize = child->first_attribute("ptsize");
        int _ptsize = ptsize ? std::atoi(ptsize->value()) : 12;

        rapidxml::xml_attribute<>* link = child->first_attribute("link");
        bool _link =
            link ? link->value() != std::string("false") ? true : false : false;

        em.link = _link;

        rapidxml::xml_attribute<>* minsize = child->first_attribute("minsize");
        glm::vec4 minSize4 =
            minsize ? Math::stringToVec4(minsize->value()) : glm::vec4(0);
        em.minSize = glm::vec2(minSize4.x, minSize4.y);

        rapidxml::xml_attribute<>* color = child->first_attribute("color");
        em.color = color   ? definedColors[color->value()]
                   : _link ? definedColors["link"]
                           : glm::vec3(1.f);

        em.font = manager->getFontCache()->get(_fontname.c_str(), _ptsize);
        if (!em.font)
          em.font = manager->getFontCache()->get("dat3/default.ttf", _ptsize);

        std::string name = std::format("{}{}", rand() % 1000, elid);
        manager->namedTextures[name].reset(
            manager->getEngine()->getDevice()->createTexture().release());
        em.texture = manager->namedTextures[name].get();
        em.dirty = true;
        em.textureHover = 0;
        em.texturePressed = 0;
        component->elementIndex.push_back(elid);
      } else if (name == "textfield") {
        rapidxml::xml_attribute<>* id = child->first_attribute("id");
        elid = id ? id->value()
                  : std::format("textfield-{}", component->elements.size());

        Component::Element& em = component->elements[elid];
        em.type = Component::Element::TextField;
        em.value = child->value();
        em.minSize = glm::vec2(0);

        rapidxml::xml_attribute<>* fontname = child->first_attribute("font");
        std::string _fontname =
            fontname ? fontname->value() : "dat3/default.ttf";

        rapidxml::xml_attribute<>* ptsize = child->first_attribute("ptsize");
        int _ptsize = ptsize ? std::atoi(ptsize->value()) : 12;

        em.font = manager->getFontCache()->get(_fontname.c_str(), _ptsize);
        if (!em.font)
          em.font = manager->getFontCache()->get("dat3/default.ttf", _ptsize);

        std::string name = std::format("{}{}", rand() % 1000, elid);
        manager->namedTextures[name].reset(
            manager->getEngine()->getDevice()->createTexture().release());
        em.texture = manager->namedTextures[name].get();
        em.dirty = true;
        em.textureHover = 0;
        em.texturePressed = 0;
        component->elementIndex.push_back(elid);
      } else if (name == "div") {
        TreeNode newNode;
        rapidxml::xml_attribute<>* id = child->first_attribute("id");
        if (id) {
          elid = id->value();
        } else {
          elid = std::format("div-{}", component->elements.size());
        }
        parseXmlNode(manager, component, &newNode, child);
        node.div = true;
        node.visible = true;
        node.children.push_back(newNode);
      } else if (name == "image") {
        rapidxml::xml_attribute<>* id = child->first_attribute("id");
        elid = id ? id->value()
                  : std::format("image-{}", component->elements.size());
        Component::Element& em = component->elements[elid];
        em.type = Component::Element::Image;
        em.color = glm::vec3(1);
        em.minSize = glm::vec2(0);

        rapidxml::xml_attribute<>* hover = child->first_attribute("hover");
        em.textureHover = hover ? manager->getEngine()
                                      ->getTextureCache()
                                      ->getOrLoad2d(hover->value())
                                      .value()
                                      .second
                                : NULL;
        rapidxml::xml_attribute<>* down = child->first_attribute("down");
        em.texturePressed = down ? manager->getEngine()
                                       ->getTextureCache()
                                       ->getOrLoad2d(down->value())
                                       .value()
                                       .second
                                 : NULL;

        auto t = manager->getEngine()
                     ->getTextureCache()
                     ->getOrLoad2d(child->first_attribute("src")->value())
                     .value();
        em.textureSize = glm::ivec2(t.first.width, t.first.height);
        em.texture = t.second;
        component->elementIndex.push_back(elid);
      } else {
        throw std::runtime_error("Unknown element");
      }
      node.elem = elid;
      parentNode->children.push_back(node);
    } catch (std::exception& e) {
      Log::printf(LOG_WARN, "Error in element %s: %s", parentXmlNode->name(),
                  e.what());
    }
  }
}

Component* GuiManager::parseXml(const char* file) {
  auto d = common::FileSystem::singleton()->getFileIO(file, "r");
  if (d) {
    size_t fileSize = d.value()->fileSize();
    char* xmld = (char*)malloc(fileSize + 4);
    memset(xmld, 0, fileSize + 4);
    size_t rd = d.value()->read(xmld, fileSize);
    try {
      rapidxml::xml_document<> doc;
      doc.parse<rapidxml::parse_full>(xmld);

      rapidxml::xml_node<>* node = doc.first_node();
      if (!node) throw std::runtime_error("Missing root node");

      rapidxml::xml_attribute<>* rootName = node->first_attribute("name");
      if (rootName) {
        auto it = components.find(rootName->value());
        if (it != components.end()) {
          return NULL;  // already exists
        }
      } else {
        throw std::runtime_error("XML root entry has no name attribute");
      }

      std::map<std::string, Component::Anchor> anchors;
      anchors["BottomLeft"] = Component::BottomLeft;
      anchors["BottomRight"] = Component::BottomRight;
      anchors["TopLeft"] = Component::TopLeft;
      anchors["TopRight"] = Component::TopRight;

      std::map<std::string, Component::GrowMode> growModes;
      growModes["Horizontal"] = Component::Horizontal;
      growModes["Vertical"] = Component::Horizontal;

      std::string name = node->first_attribute("name")->value();
      Component& component = components[name];

      component.manager = this;

      component.variablesDirty = true;
      component.name = rootName->value();

      rapidxml::xml_attribute<>* anchor = node->first_attribute("anchor");
      if (anchor) {
        component.anchor = anchors[anchor->value()];
      } else {
        component.anchor = Component::TopLeft;
      }

      rapidxml::xml_attribute<>* growMode = node->first_attribute("grow");
      if (growMode) {
        component.grow = growModes[growMode->value()];
      } else {
        component.grow = Component::Vertical;
      }

      for (rapidxml::xml_node<>* child = node->first_node(); child;
           child = child->next_sibling()) {
        std::string name = child->name();
        if (name == "component") {
          std::string src = child->first_attribute("src")->value();
          Component* newComponent = parseXml(src.c_str());
          if (newComponent) {
            newComponent->parentComponent = component.name;
            Log::printf(LOG_DEBUG, "Parented %s to %s",
                        newComponent->name.c_str(),
                        newComponent->parentComponent.c_str());
          }
        } else if (name == "script") {
          script::Script s(engine->getWorld()->getScriptContext());
          std::string contents;

          if (rapidxml::xml_attribute<>* src = child->first_attribute("src")) {
            common::OptionalData d =
                common::FileSystem::singleton()->readFile(src->value());
            if (d) {
              contents = std::string(d->begin(), d->end());
            } else {
              throw std::runtime_error("Couldn't find script src");
            }
          } else {
            contents = child->value();
          }
          component.scripts.push_back(s);
          script::Script* _s = &component.scripts[component.scripts.size() - 1];
          component.scriptUpdate(_s);
          _s->load(contents);
        } else if (name == "in_vars") {
          for (rapidxml::xml_node<>* _child = child->first_node(); _child;
               _child = _child->next_sibling()) {
            std::string name = _child->first_attribute("name")->value();
            std::string type = _child->first_attribute("type")->value();
            std::string defaultVar = _child->value();

            Component::Var v;

            std::map<std::string, Component::Var::Type> types;
            types["float"] = Component::Var::Float;
            types["int"] = Component::Var::Int;
            types["string"] = Component::Var::String;

            v.type = types[type];
            v.defaultValue = defaultVar;
            v.value = 0;
            switch (v.type) {
              case Component::Var::Float:
                v.value = new float;
                *(float*)v.value = std::atof(v.defaultValue.c_str());
                break;
              case Component::Var::Int:
                v.value = new int;
                *(int*)v.value = std::atoi(v.defaultValue.c_str());
                break;
              case Component::Var::String:
                v.value = new std::string;
                *(std::string*)v.value = v.defaultValue;
                break;
            }
            component.inVars[name] = v;
          }
        } else if (name == "dom") {
          component.domRoot.div = true;
          component.domRoot.visible = true;
          parseXmlNode(this, &component, &component.domRoot, child);
        } else {
          Log::printf(LOG_WARN, "Unknown element %s", name.c_str());
        }
      }

      component.variableChanged.listen([this, name] {
        Component* component = &components[name];
        for (auto& script : component->scripts) {
          component->scriptUpdate(&script);
          script.run();
        }
      });

      Log::printf(LOG_DEBUG, "Loaded XML %s", file);

      return &components[name];
    } catch (rapidxml::parse_error& e) {
      Log::printf(LOG_ERROR, "XML parse error: %s (%s, \"%s\")", file, e.what(),
                  e.where<char>());
#ifndef NDEBUG
      Log::printf(LOG_DEBUG, "%s", xmld);
#endif
    } catch (std::exception& e) {
      Log::printf(LOG_ERROR, "Error loading XML: %s (%s)", file, e.what());
    }

    free(xmld);
    delete d.value();
  } else {
    Log::printf(LOG_ERROR, "Error loading XML: %s", file);
  }

  return NULL;
}

void GuiManager::render() {
  engine->getDevice()->setBlendState(BaseDevice::SrcAlpha,
                                     BaseDevice::OneMinusSrcAlpha);
  BaseFrameBuffer::AttachmentPoint attachments[] = {
      BaseFrameBuffer::Color0,
      BaseFrameBuffer::Color1,
  };
  engine->getDevice()->targetAttachments(attachments, 1);

  BaseProgram* _panel = panel->prepareDevice(engine->getDevice(), 0);
  BaseProgram* _image = image->prepareDevice(engine->getDevice(), 0);
  BaseProgram* _text = text->prepareDevice(engine->getDevice(), 0);

  auto fps = getComponentByName("FPS");
  if (fps) {
    *(float*)(fps.value()->inVars["renderFps"].value) =
        1.0 / engine->getRenderJob()->getStats().getAvgDeltaTime();
    *(float*)(fps.value()->inVars["realRenderDt"].value) =
        engine->getRenderJob()->getStats().deltaTime;
    fps.value()->variablesDirty = true;
  }

  for (auto& component : components) {
    component.second.render(this, engine);
  }

  /*for (auto& layout : layouts) {
    if (!layout.second.overrideSize &&
        layout.second.size != engine->getCamera().getFramebufferSize()) {
      layout.second.update(engine->getCamera().getFramebufferSize());
    }

    for (auto& element : layout.second.getElements()) {
      BaseProgram* bp = _panel;
      switch (element.type) {
        case Element::Image:
          bp = _image;
          bp->setParameter("texture0", DtSampler,
                           BaseProgram::Parameter{
                               .texture.slot = 0,
                               .texture.texture =
                                   engine->getTextureCache()
                                       ->getOrLoad2d(element.getText().c_str())
                                       .value()
                                       .second});
        case Element::Panel:
          bp->setParameter("scale", DtVec2,
                           BaseProgram::Parameter{.vec2 = element.scale});
          bp->setParameter("offset", DtVec2,
                           BaseProgram::Parameter{.vec2 = element.offset});
          bp->bind();
          break;
        case Element::Text:
          if (!element.uniqueTexture) {
            std::unique_ptr<BaseTexture> bx =
                engine->getDevice()->createTexture();
            element.uniqueTexture = bx.get();
            textures.push_back(std::move(bx));
          }

          if (element.isDirty()) {
            std::string fontName =
                element.font + std::to_string(element.fontSize);
            auto it = fonts.find(fontName);
            if (it == fonts.end()) {
              common::OptionalData ds =
                  common::FileSystem::singleton()->readFile(
                      element.font.c_str());
              if (ds) {
                void* dcpy = malloc(ds->size());
                memcpy(dcpy, ds->data(), ds->size());
                SDL_RWops* fontMem = SDL_RWFromConstMem(dcpy, ds->size());
                TTF_Font* fontOut =
                    TTF_OpenFontRW(fontMem, 0, element.fontSize);
                if (!fontOut) {
                  throw std::runtime_error("Error creating font");
                }
                fonts[fontName] = fontOut;
              } else {
                throw std::runtime_error("I cant find the font");
              }
            }

            TTF_Font* font = (TTF_Font*)fonts[fontName];
            SDL_Color white;
            white.r = 255;
            white.g = 255;
            white.b = 255;
            white.a = 255;
            SDL_Surface* texSurf = 0;
            if (element.wrapped)
              texSurf = TTF_RenderUTF8_Blended_Wrapped(
                  font, element.getText().c_str(), white,
    element.maxScale.x); else texSurf = TTF_RenderUTF8_Blended(font,
    element.getText().c_str(), white); if (!texSurf) {
    Log::printf(LOG_DEBUG, "TTF_GetError = %s (Text: %s)", TTF_GetError(),
    element.getText().c_str()); throw std::runtime_error("Couldn't render
    TTF");
            }
            SDL_Surface* texSurfConv =
                SDL_ConvertSurfaceFormat(texSurf, SDL_PIXELFORMAT_ABGR8888,
    0); SDL_LockSurface(texSurfConv);
            element.uniqueTexture->upload2d(texSurfConv->w, texSurfConv->h,
                                            DtUnsignedByte,
    BaseTexture::RGBA, texSurfConv->pixels); if (element.override) {
              element.maxScale = glm::vec2(texSurfConv->w, texSurfConv->h);
              layout.second.update(
                  layout.second.size);  // requires layout update to take
    max
                                        // scale into account
            }
            SDL_UnlockSurface(texSurfConv);
            SDL_FreeSurface(texSurfConv);
            SDL_FreeSurface(texSurf);

            element.clearDirty();
          }

          bp = _text;
          bp->setParameter(
              "texture0", DtSampler,
              BaseProgram::Parameter{.texture.slot = 0,
                                     .texture.texture =
    element.uniqueTexture}); bp->setParameter("scale", DtVec2,
                           BaseProgram::Parameter{.vec2 = element.scale});
          bp->setParameter("offset", DtVec2,
                           BaseProgram::Parameter{.vec2 = element.offset});
          bp->bind();
      }
      squareArrayPointers->bind();
      engine->getDevice()->draw(squareElementBuffer.get(), DtUnsignedByte,
                                BaseDevice::Triangles, 6);
    }
    }*/

  engine->getDevice()->targetAttachments(attachments, 2);
}
}  // namespace rdm::gfx::gui
