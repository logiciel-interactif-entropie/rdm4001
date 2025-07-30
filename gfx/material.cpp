#include "material.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "base_device.hpp"
#include "base_types.hpp"
#include "camera.hpp"
#include "engine.hpp"
#include "filesystem.hpp"
#include "json.hpp"
#include "logging.hpp"
#include "settings.hpp"
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
  f.type = RawShaderCode;
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

Technique::Technique(BaseDevice* device, std::optional<ShaderFile> vs,
                     std::optional<ShaderFile> fs,
                     std::optional<ShaderFile> gs) {
  program = device->createProgram();
  if (vs.has_value()) {
    program->addShader(vs.value(), BaseProgram::Vertex);
  }
  if (fs.has_value()) {
    program->addShader(fs.value(), BaseProgram::Fragment);
  }
  if (gs.has_value()) {
    program->addShader(gs.value(), BaseProgram::Geometry);
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

std::shared_ptr<Technique> Technique::create(BaseDevice* device,
                                             std::optional<ShaderFile> vs,
                                             std::optional<ShaderFile> fs,
                                             std::optional<ShaderFile> gs) {
  return std::shared_ptr<Technique>(new Technique(device, vs, fs, gs));
}

Material::Material() {}

void Material::addTechnique(std::shared_ptr<Technique> qu) {
  techniques.push_back(qu);
}

BaseProgram* Material::prepareDevice(BaseDevice* device, int techniqueId) {
  if (techniques.size() <= techniqueId) return NULL;
  Engine* engine = device->getEngine();
  Camera camera = engine->getCurrentViewport()->getFrameCamera();
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
  p.texture.texture = engine->getWhiteTexture();
  p.texture.slot = 0;
  program->setParameter("white_texture", DtSampler, p);
  techniques[techniqueId]->bindProgram();
  return program;
}

std::shared_ptr<Material> Material::create() {
  return std::shared_ptr<Material>(new Material());
}

MaterialCache::MaterialCache(BaseDevice* device) {
  this->device = device;
  std::vector<unsigned char> materialJsonString =
      common::FileSystem::singleton()
          ->readFile("engine/materials/materials.json")
          .value();
  materialDatas.push_front(
      std::string(materialJsonString.begin(), materialJsonString.end()));
}

void MaterialCache::addDataFile(const char* path) {
  std::vector<unsigned char> materialJsonString =
      common::FileSystem::singleton()->readFile(path).value();
  materialDatas.push_front(
      std::string(materialJsonString.begin(), materialJsonString.end()));
}

MaterialBinaryFile::MaterialBinaryFile(std::string path) {
  Log::printf(LOG_DEBUG, "Opening material binary file %s", path.c_str());
  auto fio =
      common::FileSystem::singleton()->getFileIO(path.c_str(), "rb").value();
  char magic[9] = {0};
  fio->read(magic, 8);
  if (magic != std::string("rdmShadr")) {
    throw std::runtime_error("magic failed");
  }

  int numShaders;
  fio->read(&numShaders, sizeof(numShaders));
  int entrySize;
  fio->read(&entrySize, sizeof(entrySize));

  for (int i = 0; i < numShaders; i++) {
    fio->seek(0x1000 + (entrySize * i), SEEK_SET);
    char name[128];
    fio->read(name, 128);

    Entry entry;
    for (int i = 0; i < ShaderBinaryType::__Max; i++) {
      ShaderBinaryType sbt = (ShaderBinaryType)i;
      int size, baseOffset;
      fio->read(&size, sizeof(size));
      fio->read(&baseOffset, sizeof(baseOffset));

      size_t p = fio->tell();

      fio->seek(baseOffset, SEEK_SET);
      std::vector<char> data;
      data.resize(size);
      fio->read(data.data(), size);
      entry.binaries[sbt] = std::string(data.begin(), data.end());

      fio->seek(p, SEEK_SET);
    }
    shaderEntries[name] = entry;
  }
}

#ifndef NDEBUG
static CVar material_usebinary("material_usebinary", "1",
                               CVARF_SAVE | CVARF_GLOBAL);
#endif

std::optional<std::shared_ptr<Material>> MaterialCache::getOrLoad(
    const char* materialName) {
  auto it = cache.find(materialName);
  if (it != cache.end()) {
    return cache[materialName];
  } else {
    std::shared_ptr<Material> material = Material::create();
    try {
      for (auto materialData : materialDatas) {
        json data = json::parse(materialData);

        json materialInfo = data["Materials"][materialName];
        if (materialInfo.is_null()) {
          continue;
        }
        MaterialBinaryFile* mbf = NULL;

        if (!data["Binary"].is_null() && material_usebinary.getBool()) {
          if (binaries.find(materialData) == binaries.end()) {
            binaries[materialData].reset(
                new MaterialBinaryFile(data["Binary"]));
          }
          mbf = binaries[materialData].get();
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
            std::shared_ptr<Technique> technique = NULL;
            if (mbf) {
              std::optional<MaterialBinaryFile::Entry> vsEntry =
                  mbf->getEntry(program["VSName"]);
              std::optional<ShaderFile> vsSf;
              ShaderBinaryType preferred = device->getPreferedShaderType();
              if (vsEntry.has_value()) {
                vsSf = {vsEntry.value().binaries[preferred], program["VSName"],
                        preferred};
              }
              std::optional<MaterialBinaryFile::Entry> fsEntry =
                  mbf->getEntry(program["FSName"]);
              std::optional<ShaderFile> fsSf;
              if (fsEntry.has_value()) {
                fsSf = {fsEntry.value().binaries[preferred], program["FSName"],
                        preferred};
              }

              std::optional<ShaderFile> gsSf;
              if (program.find("GSName") != program.end()) {
                std::optional<MaterialBinaryFile::Entry> gsEntry =
                    mbf->getEntry(program["GSName"]);
                if (gsEntry.has_value()) {
                  gsSf = {gsEntry.value().binaries[preferred],
                          program["GSName"], preferred};
                }
              }
              technique = Technique::create(device, vsSf, fsSf, gsSf);
              material->addTechnique(technique);
            } else {
              Log::printf(LOG_DEBUG, "Loading material %s without binary",
                          materialName);

              std::string vsName = program["VSName"];
              std::string fsName = program["FSName"];
              std::string gsName = "";
              if (program.find("GSName") != program.end())
                gsName = program["GSName"];
              technique = Technique::create(device, vsName, fsName, gsName);
              material->addTechnique(technique);
            }

            if (!program["Bindings"].is_null()) {
              for (auto& [key, value] : program["Bindings"].items()) {
                technique->getProgram()->addBinding(key, value);
              }
            } else {
              Log::printf(LOG_WARN, "Program %s has no binding list",
                          programName.c_str());
            }
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
      }
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
