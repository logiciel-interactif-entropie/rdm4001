#include "material.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

#include "camera.hpp"
#include "engine.hpp"
#include "filesystem.hpp"
#include "json.hpp"
#include "logging.hpp"
using json = nlohmann::json;

namespace rdm::gfx {
static ShaderCache* _singleton;
ShaderCache* ShaderCache::singleton() {
  if (!_singleton) {
    _singleton = new ShaderCache();
  }
  return _singleton;
}

ShaderFile ShaderCache::getCachedOrFile(const char* path) {
  auto it = cache.find(path);
  ShaderFile f;
  f.name = path;
  if (it != cache.end()) {
    f.code = cache[path];
  } else {
    common::OptionalData data = common::FileSystem::singleton()->readFile(path);
    if (data) {
      std::string code = std::string(data->begin(), data->end());
      cache[path] = code;
      f.code = code;
    } else {
      Log::printf(LOG_ERROR, "Could not load shader file %s", path);
      throw std::runtime_error("Could not load shader file");
    }
  }
  return f;
}

Technique::Technique(BaseDevice* device, std::string techniqueVs,
                     std::string techniqueFs, std::string techniqueGs) {
  program = device->createProgram();
  program->addShader(
      ShaderCache::singleton()->getCachedOrFile(techniqueVs.c_str()),
      BaseProgram::Vertex);
  program->addShader(
      ShaderCache::singleton()->getCachedOrFile(techniqueFs.c_str()),
      BaseProgram::Fragment);
  if (!techniqueGs.empty()) {
    program->addShader(
        ShaderCache::singleton()->getCachedOrFile(techniqueGs.c_str()),
        BaseProgram::Geometry);
  }
  program->link();
}

void Technique::bindProgram() { program->bind(); }

std::shared_ptr<Technique> Technique::create(BaseDevice* device,
                                             std::string techniqueVs,
                                             std::string techniqueFs,
                                             std::string techniqueGs) {
  return std::shared_ptr<Technique>(
      new Technique(device, techniqueVs, techniqueFs, techniqueGs));
}

Material::Material() {}

void Material::addTechnique(std::shared_ptr<Technique> qu) {
  techniques.push_back(qu);
}

BaseProgram* Material::prepareDevice(BaseDevice* device, int techniqueId) {
  if (techniques.size() <= techniqueId) return NULL;
  Engine* engine = device->getEngine();
  Camera camera = engine->getCurrentViewport()->getCamera();
  BaseProgram* program = techniques[techniqueId]->getProgram();
  BaseProgram::Parameter p;
  p.matrix4x4 = camera.getProjectionMatrix();
  program->setParameter("projectionMatrix", DtMat4, p);
  p.matrix4x4 = camera.getUiProjectionMatrix();
  program->setParameter("uiProjectionMatrix", DtMat4, p);
  p.matrix4x4 = camera.getViewMatrix();
  program->setParameter("viewMatrix", DtMat4, p);
  p.number = device->getEngine()->getTime();
  program->setParameter("time", DtFloat, p);
  p.vec3 = camera.getPosition();
  program->setParameter("camera_position", DtVec3, p);
  p.vec3 = camera.getTarget();
  program->setParameter("camera_target", DtVec3, p);
  techniques[techniqueId]->bindProgram();
  return program;
}

std::shared_ptr<Material> Material::create() {
  return std::shared_ptr<Material>(new Material());
}

MaterialCache::MaterialCache(BaseDevice* device) {
  this->device = device;
  std::vector<unsigned char> materialJsonString =
      common::FileSystem::singleton()->readFile("dat1/materials.json").value();
  materialData =
      std::string(materialJsonString.begin(), materialJsonString.end());
}

std::optional<std::shared_ptr<Material>> MaterialCache::getOrLoad(
    const char* materialName) {
  auto it = cache.find(materialName);
  if (it != cache.end()) {
    return cache[materialName];
  } else {
    std::shared_ptr<Material> material = Material::create();
    try {
      json data = json::parse(materialData);
      json materialInfo = data["Materials"][materialName];
      if (materialInfo.is_null()) {
        Log::printf(LOG_ERROR, "Could not find material %s", materialName);
        return {};
      }
      json materialRequirements = data["Materials"][materialName];
      if (!materialRequirements.is_null()) {
        if (materialRequirements["PostProcess"].is_boolean()) {
          if (materialRequirements["PostProcess"]) {
          }
        }
      }
      json techniques = materialInfo["Techniques"];
      int techniqueId = 0;
      for (const json& item : techniques) {
        std::string programName = item["ProgramName"];
        json program = data["Programs"][programName];
        try {
          if (program.is_null()) {
            Log::printf(LOG_ERROR, "Could not find program for technique %i",
                        techniqueId);
            continue;
          }
          std::string vsName = program["VSName"];
          std::string fsName = program["FSName"];
          std::string gsName = "";
          if (program.find("GSName") != program.end())
            gsName = program["GSName"];
          material->addTechnique(
              Technique::create(device, vsName, fsName, gsName));
        } catch (std::runtime_error& e) {
          Log::printf(
              LOG_ERROR,
              "Couldn't compile Technique %i for material %s what() = %s",
              techniqueId, materialName, e.what());
          continue;
        }
        techniqueId++;
      }
      Log::printf(LOG_DEBUG, "Cached new material %s", materialName);
      cache[materialName] = material;
      return material;
    } catch (std::runtime_error& e) {
      Log::printf(LOG_ERROR,
                  "Fucked up MaterialCache::getOrLoad sorry  for making it "
                  "json what() = ",
                  e.what());
      return {};
    }
  }
}
}  // namespace rdm::gfx
