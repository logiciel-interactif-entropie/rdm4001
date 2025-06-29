#pragma once
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base_device.hpp"
#include "base_types.hpp"

namespace rdm::gfx {
/**
 * @brief Cache for shader source files (not compiled shader objects).
 *
 * @todo This should store compiled shared objects.
 *
 */
class ShaderCache {
  std::map<std::string, std::string> cache;

 public:
  static ShaderCache* singleton();

  /**
   * @brief Retrieves a source file from the cache, or loads it in.
   *
   * @param path Path to the shader relative to the data directory
   * @return ShaderFile The loaded shader file. If not found, it will throw a
   * std::runtime_error
   */
  ShaderFile getCachedOrFile(const char* path);
};

/**
 * @brief Individual pass for a Material
 *
 * This stores 1 gfx::BaseProgram, and will be bound before a
 * Entity::renderTechnique
 */
class Technique {
  std::unique_ptr<BaseProgram> program;
  Technique(BaseDevice* device, std::string techniqueVs,
            std::string techniqueFs, std::string techniqueGs);

 public:
  static std::shared_ptr<Technique> create(BaseDevice* device,
                                           std::string techniqueVs,
                                           std::string techniqueFs,
                                           std::string techniqueGs = "");

  void bindProgram();
  BaseProgram* getProgram() { return program.get(); }
};

/**
 * @brief Materials store gfx::Technique's and are required to render with
 * Entity's.
 *
 * This requires Techniques to load.
 */
class Material {
  std::vector<std::shared_ptr<Technique>> techniques;

  Material();

 public:
  /**
   * @brief You will likely want to use MaterialCache rather then this.
   *
   * See MaterialCache::getOrLoad. The engine will give you a MaterialCache*,
   * see Engine::getMaterialCache.
   *
   * @return std::shared_ptr<Material> The new material.
   */
  static std::shared_ptr<Material> create();

  void addTechnique(std::shared_ptr<Technique> qu);

  int numTechniques() { return techniques.size(); }
  BaseProgram* prepareDevice(BaseDevice* device, int techniqueId);
};

/**
 * @brief MaterialCache caches all the materials. Yay
 *
 */
class MaterialCache {
  std::map<std::string, std::shared_ptr<Material>> cache;
  std::list<std::string> materialDatas;
  BaseDevice* device;

 public:
  MaterialCache(BaseDevice* device);
  void addDataFile(const char* path);

  /**
   * @brief Loads and caches a material, or just returns an already cached
   * material.
   *
   * @param materialName The name of the material as defined in
   * dat1/materials.json
   * @return std::optional<std::shared_ptr<Material>> The loaded material
   */
  std::optional<std::shared_ptr<Material>> getOrLoad(const char* materialName);
};
}  // namespace rdm::gfx
