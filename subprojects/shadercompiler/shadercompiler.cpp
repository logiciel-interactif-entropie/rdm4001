#include <shaderc/env.h>
#include <shaderc/shaderc.h>
#include <stdlib.h>

#include <cstdio>
#include <cstdlib>
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
    includePaths.push_back(argv[i]);
  }

  FILE* jsonFile = fopen(materialDefinitionDir.c_str(), "r");
  if (!jsonFile) {
    printf("could not open json file %s\n", materialDefinitionDir.c_str());
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
      }

      std::string shaderPath = shaders.value();
      if (compiledShaders.find(shaderPath) != compiledShaders.end()) continue;

      CompiledShader shader;
      std::string shaderData;
      std::ifstream stream(dataDir + "/" + shaderPath);
      if (!stream.is_open()) {
        printf("error: could not open file %s\n", shaderPath.c_str());
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

      shaderc::PreprocessedSourceCompilationResult presult =
          compiler.PreprocessGlsl(shaderData, kind, shaderPath.c_str(),
                                  options);
      if (presult.GetCompilationStatus() !=
          shaderc_compilation_status_success) {
        printf("error: shader preprocessing error %s\n%s", shaderPath.c_str(),
               presult.GetErrorMessage().c_str());
        failed++;
        continue;
      } else {
        shader.glsl = std::string(presult.begin(), presult.end());
      }

      shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
          shaderData, kind, shaderPath.c_str(), options);
      if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        printf("error: shader compilation error %s\n%s", shaderPath.c_str(),
               result.GetErrorMessage().c_str());
      } else {
        shader.spirv = std::vector<uint32_t>(result.begin(), result.end());
      }

      compiled++;
      compiledShaders[shaderPath] = shader;
    }
  }

  printf("%i shaders compiled, %i failed, %i tried\n", compiled, failed, tried);

  FILE* output =
      fopen((dataDir + "/" + (std::string)data["Binary"]).c_str(), "wb");
  if (!output) {
    printf("error: could not open output file %s\n", strerror(errno));
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
  std::string warning = "DON'T LOOK AT MY FILE";
  fwrite(warning.data(), warning.size(), 1, output);
  int numShaders = compiledShaders.size();
  fwrite(&numShaders, sizeof(numShaders), 1, output);
  fwrite(&entrySize, sizeof(entrySize), 1, output);

  return 0;
}
