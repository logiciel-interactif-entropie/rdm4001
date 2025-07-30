#include <shaderc/env.h>
#include <shaderc/shaderc.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <shaderc/shaderc.hpp>
#include <sstream>

#include "json.hpp"
using json = nlohmann::json;

struct CompiledShader {
  std::string glsl;
  std::vector<uint32_t> spirv;
};

class ShadercIncluder : public shaderc::CompileOptions::IncluderInterface {
  std::vector<std::string> includePaths;

 public:
  ShadercIncluder(std::vector<std::string> includePaths) {
    fprintf(stderr, "ShadercIncluder %i\n", includePaths.size());
    this->includePaths = includePaths;
  }

  virtual shaderc_include_result* GetInclude(const char* requested_source,
                                             shaderc_include_type type,
                                             const char* requesting_source,
                                             size_t include_depth) {
    if (!includePaths.size()) fprintf(stderr, "No include paths!!\n");

    for (auto& incPath : includePaths) {
      std::string path = incPath + "/" + requested_source;
      std::ifstream stream(path);
      if (!stream.is_open()) {
        continue;
      }

      shaderc_include_result* result = new shaderc_include_result();
      std::ostringstream ss;
      ss << stream.rdbuf();
      std::string data = ss.str();

      result->content = (const char*)malloc(data.size() + 1);
      strcpy((char*)result->content, data.data());
      result->content_length = data.size();

      result->source_name = (const char*)malloc(path.size() + 1);
      strcpy((char*)result->source_name, path.data());
      result->source_name_length = path.size();

      return result;
    }

    shaderc_include_result* result = new shaderc_include_result();
    result->user_data = (void*)0xfff;
    return result;
  }

  virtual void ReleaseInclude(shaderc_include_result* data) {
    if ((uint64_t)data->user_data == 0xfff) {
      return;
    }
    free((void*)data->content);
    free((void*)data->source_name);
  }
};

int main(int argc, char** argv) {
  if (argc == 1) {
    printf(
        "%s <data directory> <material json path, relative from data "
        "directory>\n",
        argv[0]);
    exit(EXIT_FAILURE);
  }

  std::string dataDir, materialDefinitionDir;
  dataDir = argv[1];
  materialDefinitionDir = argv[2];
  materialDefinitionDir = dataDir + "/" + materialDefinitionDir;

  std::vector<std::string> includePaths;

  for (int i = 3; i < argc; i++) {
    fprintf(stderr, "Include path: %s\n", argv[i]);
    includePaths.push_back(argv[i]);
  }

  FILE* jsonFile = fopen(materialDefinitionDir.c_str(), "r");
  if (!jsonFile) {
    fprintf(stderr, "could not open json file %s\n",
            materialDefinitionDir.c_str());
    exit(EXIT_FAILURE);
  }
  fseek(jsonFile, 0, SEEK_END);
  size_t jsonFileSize = ftell(jsonFile);
  fseek(jsonFile, 0, SEEK_SET);
  std::string jsonData;
  jsonData.reserve(jsonFileSize);
  fread(jsonData.data(), jsonFileSize, 1, jsonFile);
  fclose(jsonFile);

  std::map<std::string, CompiledShader> compiledShaders;

  int tried = 0;
  int failed = 0;
  int compiled = 0;

  json data = json::parse(jsonData.data());

  for (auto& programs : data["Programs"].items()) {
    for (auto& shaders : programs.value().items()) {
      tried++;

      shaderc_shader_kind kind;
      std::string shaderKey = shaders.key();
      if (shaderKey == "VSName") {
        kind = shaderc_vertex_shader;
      } else if (shaderKey == "GSName") {
        kind = shaderc_geometry_shader;
      } else if (shaderKey == "FSName") {
        kind = shaderc_fragment_shader;
      } else {
        continue;
      }

      std::string shaderPath = shaders.value();
      if (compiledShaders.find(shaderPath) != compiledShaders.end()) continue;

      CompiledShader shader;
      std::string shaderData;
      std::ifstream stream(dataDir + "/" + shaderPath);
      if (!stream.is_open()) {
        fprintf(stderr, "error: could not open file %s\n", shaderPath.c_str());
        failed++;
        continue;
      }
      std::ostringstream ss;
      ss << stream.rdbuf();
      shaderData = ss.str();

      shaderc::Compiler compiler;
      shaderc::CompileOptions options;
      options.SetSourceLanguage(shaderc_source_language_glsl);
      options.SetOptimizationLevel(shaderc_optimization_level_performance);
      options.SetTargetEnvironment(shaderc_target_env_opengl, 3300);
      options.SetAutoMapLocations(true);
      options.SetAutoBindUniforms(true);
      options.SetIncluder(
          std::unique_ptr<shaderc::CompileOptions::IncluderInterface>(
              new ShadercIncluder(includePaths)));

      shaderc::PreprocessedSourceCompilationResult presult =
          compiler.PreprocessGlsl(shaderData, kind, shaderPath.c_str(),
                                  options);
      if (presult.GetCompilationStatus() !=
          shaderc_compilation_status_success) {
        fprintf(stderr, "error: shader preprocessing error %s\n%s",
                shaderPath.c_str(), presult.GetErrorMessage().c_str());
        failed++;
        continue;
      } else {
        shader.glsl = std::string(presult.begin(), presult.end());
      }

      shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
          shaderData, kind, shaderPath.c_str(), options);
      if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        fprintf(stderr, "error: shader compilation error %s\n%s",
                shaderPath.c_str(), result.GetErrorMessage().c_str());
      } else {
        shader.spirv = std::vector<uint32_t>(result.begin(), result.end());
      }

      compiled++;
      compiledShaders[shaderPath] = shader;
    }
  }

  printf("%i shaders compiled, %i failed, %i tried\n", compiled, failed, tried);
  for (auto& [nam, _] : compiledShaders) {
    printf("%s\n", nam.c_str());
  }

  FILE* output =
      fopen((dataDir + "/" + (std::string)data["Binary"]).c_str(), "wb");
  if (!output) {
    fprintf(stderr, "error: could not open output file %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  int baseOffset = 0x6777;
  fseek(output, 0x1000, SEEK_SET);
  size_t entrySize = 0;
  for (auto& [name, shader] : compiledShaders) {
    char shaderName[128];
    size_t beginning = ftell(output);
    strncpy(shaderName, name.c_str(), 128);
    fwrite(shaderName, 128, 1, output);

    // write preprocessed glsl shader

    int glslSize = shader.glsl.size();

    fwrite(&glslSize, sizeof(glslSize), 1, output);
    fwrite(&baseOffset, sizeof(baseOffset), 1, output);

    size_t last = ftell(output);
    fseek(output, baseOffset, SEEK_SET);
    fwrite(shader.glsl.data(), glslSize, 1, output);
    baseOffset = ftell(output);
    fseek(output, last, SEEK_SET);

    // write spir-v binary

    int sprvSize = shader.spirv.size();

    fwrite(&sprvSize, sizeof(sprvSize), 1, output);
    fwrite(&baseOffset, sizeof(baseOffset), 1, output);

    last = ftell(output);
    fseek(output, baseOffset, SEEK_SET);
    fwrite(shader.spirv.data(), sprvSize, 1, output);
    baseOffset = ftell(output);
    fseek(output, last, SEEK_SET);

    entrySize = ftell(output) - beginning;
  }

  fseek(output, 0, SEEK_SET);
  std::string warning = "rdmShadr";
  fwrite(warning.data(), warning.size(), 1, output);
  int numShaders = compiledShaders.size();
  fwrite(&numShaders, sizeof(numShaders), 1, output);
  fwrite(&entrySize, sizeof(entrySize), 1, output);

  return 0;
}
