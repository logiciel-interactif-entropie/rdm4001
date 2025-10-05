#include <assimp/material.h>
#include <assimp/types.h>

#include <algorithm>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

#include "object.hpp"
#include "object_property.hpp"
#include "script/script_api.hpp"
#include "subprojects/common/logging.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/stb_image.h"
#include "resource.hpp"
#include "settings.hpp"

namespace rdm::resource {
RDM_REFLECTION_BEGIN_DESCRIBED(Model);
RDM_REFLECTION_PROPERTY_FUNCTION(Model, Render, [](lua_State* L) {
  Model* model = script::ObjectBridge::getDescribed<Model>(L, 1);
  gfx::Engine* engine = script::ObjectBridge::getDescribed<gfx::Engine>(L, 2);
  model->render(engine->getDevice());
  return 0;
})
RDM_REFLECTION_END_DESCRIBED();

static CVar mdl_render_deferred("mdl_render_deferred", "0", CVARF_GLOBAL);

Model::Model(ResourceManager* rm, std::string name)
    : BaseGfxResource(rm, name) {
  path = std::string(name).find_last_of('/');
  broken = true;
  boneCount = 0;
  boundingBox.max = glm::vec3(0.0);
  boundingBox.min = glm::vec3(0.0);
}

void Model::gfxDelete() {}

class AssimpIOStream : public Assimp::IOStream {
  common::FileIO* io;

 public:
  AssimpIOStream(common::FileIO* io) { this->io = io; }
  ~AssimpIOStream() { delete io; }

  virtual size_t Read(void* buf, size_t size, size_t count) {
    size_t c = io->read(buf, size * count);
    return c / size;
  }

  virtual size_t Write(const void* buf, size_t size, size_t count) {
    return io->write(buf, size * count);
  }

  virtual aiReturn Seek(size_t offset, aiOrigin origin) {
    io->seek(offset, origin);
    return aiReturn_SUCCESS;
  }

  virtual size_t Tell() const { return io->tell(); }

  virtual size_t FileSize() const { return io->fileSize(); }

  virtual void Flush() {
    // TODO
    // But probably never bother because we'll never be exporting models ever
  }
};

class AssimpIOSystem : public Assimp::IOSystem {
  std::string basePath;

 public:
  AssimpIOSystem(std::string basePath) { this->basePath = basePath; }

  virtual bool Exists(const char* file) const {
    return common::FileSystem::singleton()
        ->getFileIO((basePath + file).c_str(), "r")
        .has_value();
  }

  virtual char getOsSeparator() const { return '/'; }

  virtual Assimp::IOStream* Open(const char* file, const char* mode) {
    auto io = common::FileSystem::singleton()->getFileIO(
        (basePath + file).c_str(), mode);
    if (io) {
      return new AssimpIOStream(io.value());
    } else
      return NULL;
  }

  virtual void Close(Assimp::IOStream* file) {}
};

void Model::gfxUpload(gfx::Engine* engine) {
  std::scoped_lock l(m);

  if (broken) {
    setReady();
    return;
  }

  for (int i = 0; i < deferedTextures.size(); i++) {
    Texture* texture = deferedTextures[i];
    aiTexture* textureData = scene->mTextures[texture->textureId];
    texture->texture = engine->getDevice()->createTexture();

    if (textureData->mHeight == 0) {
      int width, height, channels;
      stbi_uc* uc = stbi_load_from_memory((stbi_uc*)textureData->pcData,
                                          textureData->mWidth, &width, &height,
                                          &channels, 4);
      texture->texture->upload2d(width, height, gfx::DtUnsignedByte,
                                 gfx::BaseTexture::RGBA, uc);
      stbi_image_free(uc);
    } else {
      texture->texture->upload2d(textureData->mWidth, textureData->mHeight,
                                 gfx::DtUnsignedByte, gfx::BaseTexture::RGBA,
                                 (void*)textureData->pcData);
    }
  }

  Log::printf(LOG_DEBUG, "Loaded %i textures", deferedTextures.size());
  deferedTextures.clear();

  for (int mesh_id = 0; mesh_id < scene->mNumMeshes; mesh_id++) {
    aiMesh* mesh = scene->mMeshes[mesh_id];
    gfx::Mesh meshData;
    meshData.skinned = mesh->HasBones();
    meshData.element = engine->getDevice()->createBuffer();
    meshData.vertex = engine->getDevice()->createBuffer();
    meshData.arrayPointers = engine->getDevice()->createArrayPointers();
    meshData.material =
        scene->mMaterials[mesh->mMaterialIndex]->GetName().C_Str();

    for (int i = 0; i < mesh->mNumFaces; i++) {
      aiFace face = mesh->mFaces[i];
      for (int j = 0; j < face.mNumIndices; j++) {
        meshData.indices.push_back(face.mIndices[j]);
      }
    }

    meshData.element->upload(gfx::BaseBuffer::Element,
                             gfx::BaseBuffer::StaticDraw,
                             meshData.indices.size() * sizeof(unsigned int),
                             meshData.indices.data());

    if (meshData.skinned) {
      std::vector<gfx::MeshVertexSkinned> vertices;
      for (int i = 0; i < mesh->mNumVertices; i++) {
        gfx::MeshVertexSkinned vertex;
        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y,
                                    mesh->mVertices[i].z);
        vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y,
                                  mesh->mNormals[i].z);
        vertex.uv = glm::vec2(mesh->mTextureCoords[0][i].x,
                              1.0 - mesh->mTextureCoords[0][i].y);
        vertex.boneIds[0] = -1;
        vertex.boneIds[1] = -1;
        vertex.boneIds[2] = -1;
        vertex.boneIds[3] = -1;

        vertex.boneWeights[0] = 0.0;
        vertex.boneWeights[1] = 0.0;
        vertex.boneWeights[2] = 0.0;
        vertex.boneWeights[3] = 0.0;

        vertices.push_back(vertex);
      }

      for (int i = 0; i < mesh->mNumBones; i++) {
        aiBone* bone = mesh->mBones[i];
        int boneId = -1;
        if (boneInfo.find(bone->mName.C_Str()) != boneInfo.end()) {
          boneId = boneInfo[bone->mName.C_Str()].id;
        } else
          Log::printf(LOG_WARN, "Mesh ref unknown bone %s",
                      bone->mName.C_Str());

        for (int j = 0; j < bone->mNumWeights; j++) {
          aiVertexWeight weight = bone->mWeights[j];
          int vertex = weight.mVertexId;
          float value = weight.mWeight;

          if (value == 0.0) continue;

          for (int z = 0; z < MODEL_MAX_WEIGHTS; z++) {
            if (vertices[vertex].boneIds[z] < 0) {
              vertices[vertex].boneIds[z] = boneId;
              vertices[vertex].boneWeights[z] = value;
              break;
            }
          }
        }
      }

      meshData.vertex->upload(
          gfx::BaseBuffer::Array, gfx::BaseBuffer::StaticDraw,
          vertices.size() * sizeof(gfx::MeshVertexSkinned), vertices.data());
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtFloat, 0, 3, sizeof(gfx::MeshVertexSkinned),
          (void*)offsetof(gfx::MeshVertexSkinned, position),
          meshData.vertex.get()));
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtFloat, 1, 3, sizeof(gfx::MeshVertexSkinned),
          (void*)offsetof(gfx::MeshVertexSkinned, normal),
          meshData.vertex.get()));
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtFloat, 2, 2, sizeof(gfx::MeshVertexSkinned),
          (void*)offsetof(gfx::MeshVertexSkinned, uv), meshData.vertex.get()));
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtInt, 3, 4, sizeof(gfx::MeshVertexSkinned),
          (void*)offsetof(gfx::MeshVertexSkinned, boneIds),
          meshData.vertex.get()));
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtFloat, 4, 4, sizeof(gfx::MeshVertexSkinned),
          (void*)offsetof(gfx::MeshVertexSkinned, boneWeights),
          meshData.vertex.get()));
    } else {
      std::vector<gfx::MeshVertex> vertices;
      for (int i = 0; i < mesh->mNumVertices; i++) {
        gfx::MeshVertex vertex;
        vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y,
                                    mesh->mVertices[i].z);
        vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y,
                                  mesh->mNormals[i].z);
        vertex.uv = glm::vec2(mesh->mTextureCoords[0][i].x,
                              1.0 - mesh->mTextureCoords[0][i].y);
        vertices.push_back(vertex);
      }
      meshData.vertex->upload(
          gfx::BaseBuffer::Array, gfx::BaseBuffer::StaticDraw,
          vertices.size() * sizeof(gfx::MeshVertex), vertices.data());
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtFloat, 0, 3, sizeof(gfx::MeshVertex),
          (void*)offsetof(gfx::MeshVertex, position), meshData.vertex.get()));
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtFloat, 1, 3, sizeof(gfx::MeshVertex),
          (void*)offsetof(gfx::MeshVertex, normal), meshData.vertex.get()));
      meshData.arrayPointers->addAttrib(gfx::BaseArrayPointers::Attrib(
          gfx::DtFloat, 2, 2, sizeof(gfx::MeshVertex),
          (void*)offsetof(gfx::MeshVertex, uv), meshData.vertex.get()));
    }

    meshData.arrayPointers->upload();
    meshes[mesh->mName.C_Str()] = std::move(meshData);
  }

  for (auto& [name, material] : materials) {
    material.pbrData = engine->getDevice()->createBuffer();
    Material::Upload upload = Material::Upload(material);
    material.pbrData->upload(gfx::BaseBuffer::Uniform,
                             gfx::BaseBuffer::StaticDraw,
                             sizeof(Material::Upload), &upload);
    Log::printf(LOG_DEBUG, "Uploaded information for %s", name.c_str());
  }

  std::string materialName = skinned ? "MeshSkinned" : "Mesh";
  gfx_material = engine->getMaterialCache()
                     ->getOrLoad((materialName + "NoDF").c_str())
                     .value();
  gfx_materialDf =
      engine->getMaterialCache()->getOrLoad(materialName.c_str()).value();

  setReady();
}

void Model::onLoadData(common::OptionalData data) {
  namespace fs = std::filesystem;
  fs::path path = getName();
  broken = false;
  if (path.extension() == "cqc") {
  } else {
    std::string dir = getName().substr(0, getName().find_last_of('/') + 1);
    AssimpIOSystem* system = new AssimpIOSystem(dir);
    importer.SetIOHandler(system);
#ifdef _WIN32
    char xtnsion[64];
    wcstombs(xtnsion, path.extension().c_str(), sizeof(xtnsion));
    scene = importer.ReadFileFromMemory(
        data->data(), data->size(), aiProcess_Triangulate | aiProcess_FlipUVs,
        xtnsion);
#else
    scene = importer.ReadFileFromMemory(
        data->data(), data->size(), aiProcess_Triangulate | aiProcess_FlipUVs,
        path.extension().c_str());
#endif
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
        !scene->mRootNode) {
      Log::printf(LOG_ERROR, "Assimp import error: %s",
                  importer.GetErrorString());
      broken = true;
      return;
    }
    inverseGlobalTransform = glm::inverse(
        gfx::ConvertMatrixToGLMFormat(scene->mRootNode->mTransformation));
    for (int i = 0; i < scene->mNumMaterials; i++) {
      aiMaterial* material = scene->mMaterials[i];
      Material& matData = materials[material->GetName().C_Str()];
      if (aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR,
                             &matData.roughness) != AI_SUCCESS) {
        Log::printf(LOG_DEBUG, "Unable to load material roughness");
        matData.roughness = 1.f;
      }
      if (aiGetMaterialFloat(material, AI_MATKEY_METALLIC_FACTOR,
                             &matData.metallic) != AI_SUCCESS) {
        Log::printf(LOG_DEBUG, "Unable to load material metallic");
        matData.metallic = 0.f;
      }
      if (aiGetMaterialFloat(material, AI_MATKEY_SPECULAR_FACTOR,
                             &matData.specular) != AI_SUCCESS) {
        Log::printf(LOG_DEBUG, "Unable to load material specular");
        matData.specular = 0.f;
      }
      matData.rimLight = 0.f;

      aiColor4D color;
      if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color) !=
          AI_SUCCESS) {
        Log::printf(LOG_DEBUG, "Unable to load material diffuse");
      }

      matData.albedo = glm::vec3(color.r, color.g, color.b);
      matData.hasAlbedo = false;

      if (material->GetTextureCount(aiTextureType_DIFFUSE)) {
        Texture& texture = matData.diffuse;
        matData.hasAlbedo = true;
        texture.texture_ref = NULL;
        std::string texturePath;
        {
          aiString aiTexturePath;
          material->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexturePath);

          if (aiTexturePath.C_Str()[0] == '*') {
            texturePath = aiTexturePath.C_Str();
            texture.textureId = std::atoi(texturePath.substr(1).c_str());
            texture.external = false;

            // defer loading this
            deferedTextures.push_back(&texture);
          } else {
            texture.external = true;
            texturePath = dir + aiTexturePath.C_Str();
            matData.diffuse.texture_ref =
                getResourceManager()->load<resource::Texture>(
                    texturePath.c_str());
            if (!matData.diffuse.texture_ref)
              Log::printf(LOG_ERROR,
                          "Could not load diffuse texture %s for material %s",
                          texturePath.c_str(), material->GetName().C_Str());
          }
        }
      }
    }
    for (int i = 0; i < scene->mNumMeshes; i++) {
      aiMesh* mesh = scene->mMeshes[i];
      std::string meshName = mesh->mName.C_Str();

      for (int i = 0; i < mesh->mNumVertices; i++) {
        glm::vec3 position = glm::vec3(
            mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        boundingBox.max = glm::max(boundingBox.max, position);
        boundingBox.min = glm::min(boundingBox.min, position);
      }

      if (mesh->HasBones()) {
        skinned = true;

        for (int j = 0; j < mesh->mNumBones; j++) {
          aiBone* bone = mesh->mBones[j];
          std::string boneName = bone->mName.C_Str();
          if (boneInfo.find(boneName) != boneInfo.end()) continue;

          gfx::BoneInfo boneData;
          boneData.id = boneCount++;
          boneData.offset = gfx::ConvertMatrixToGLMFormat(bone->mOffsetMatrix);
          // boneData.offset = glm::identity<glm::mat4>();
          boneInfo[boneName] = boneData;
        }
      }
    }
    for (int i = 0; i < scene->mNumAnimations; i++) {
      aiAnimation* anim = scene->mAnimations[i];
      Animation animData;

      animData.tps = anim->mTicksPerSecond;
      animData.duration = anim->mDuration;
      for (int j = 0; j < anim->mNumChannels; j++) {
        aiNodeAnim* nodeAnim = anim->mChannels[j];

        if (skinned &&
            boneInfo.find(nodeAnim->mNodeName.C_Str()) == boneInfo.end()) {
          gfx::BoneInfo& info = boneInfo[nodeAnim->mNodeName.C_Str()];
          info.id = boneCount++;
          info.offset = glm::identity<glm::mat4>();
          Log::printf(LOG_WARN, "Anim %s adds missing bone %s",
                      nodeAnim->mNodeName.C_Str());
        }

        BoneKeyframe& chan_keys =
            animData.boneKeys[nodeAnim->mNodeName.C_Str()];
        chan_keys.translations.reserve(nodeAnim->mNumPositionKeys);
        chan_keys.rotations.reserve(nodeAnim->mNumRotationKeys);
        chan_keys.scales.reserve(nodeAnim->mNumScalingKeys);

        for (int j = 0; j < nodeAnim->mNumPositionKeys; j++) {
          KeyTranslate k;
          aiVector3D p = nodeAnim->mPositionKeys[j].mValue;
          k.position = glm::vec3(p.x, p.y, p.z);
          k.timestamp = nodeAnim->mPositionKeys[j].mTime;
          chan_keys.translations.push_back(k);
        }

        for (int j = 0; j < nodeAnim->mNumScalingKeys; j++) {
          KeyScale k;
          aiVector3D p = nodeAnim->mScalingKeys[j].mValue;
          k.scale = glm::vec3(p.x, p.y, p.z);
          k.timestamp = nodeAnim->mScalingKeys[j].mTime;
          chan_keys.scales.push_back(k);
        }

        for (int j = 0; j < nodeAnim->mNumRotationKeys; j++) {
          KeyRotate k;
          aiQuaternion p = nodeAnim->mRotationKeys[j].mValue;
          k.quat = glm::quat(p.w, p.x, p.y, p.z);
          k.timestamp = nodeAnim->mRotationKeys[j].mTime;
          chan_keys.rotations.push_back(k);
        }
      }

      animations[anim->mName.C_Str()] = animData;
      if (!preferedAnimation)
        preferedAnimation = &animations[anim->mName.C_Str()];
    }
  }
}

void Model::render(
    gfx::BaseDevice* device, Animator* animator, gfx::Material* material,
    std::optional<std::function<void(gfx::BaseProgram*)>> setParameters) {
  if (!getReady()) return;
  if (meshes.size() == 0)
    throw std::runtime_error("There are no meshes in this model");
  {
    std::scoped_lock l(m);

    bool useDf = !device->getEngine()->getMaterialCache()->getPreferNoDF() &&
                 mdl_render_deferred.getBool();
    gfx::Material* usedMaterial =
        material ? material
                 : (useDf ? gfx_materialDf.get() : gfx_material.get());
    gfx::BaseProgram* bp = usedMaterial->prepareDevice(device, 0);

    if (!bp) throw std::runtime_error("bp == NULL");

    device->getEngine()->getCurrentViewport()->getLightingManager().upload(
        glm::vec3(0), bp);
    if (skinned && animator) animator->upload(bp);
    if (setParameters) setParameters.value()(bp);

    for (auto& [name, mesh] : meshes) {
      Material& mat = materials[mesh.material];
      gfx::BaseTexture* texture =
          mat.hasAlbedo
              ? (mat.diffuse.external ? mat.diffuse.texture_ref->getTexture()
                                      : mat.diffuse.texture.get())
              : device->getEngine()->getWhiteTexture();
      bp->setParameter("albedo_texture", gfx::DtSampler,
                       {
                           .texture = {.slot = 0, .texture = texture},
                       });
      bp->setParameter("Material", gfx::DtBuffer,
                       {.buffer = {.slot = 0, .buffer = mat.pbrData.get()}});
      bp->bind();
      mesh.render(device);
    }
  }
}

void Model::updateAnimator(gfx::Engine* engine, Animator* anim) {
  if (!skinned)
    throw std::runtime_error("Calling updateAnimator on unskinned model");
  if (!anim->animation) return;
  anim->currentTime += anim->animation->tps *
                       engine->getRenderJob()->getStats().deltaTime *
                       anim->speed;
  anim->currentTime = fmod(anim->currentTime, anim->animation->duration);
  calcAnimatorTransforms(scene->mRootNode, anim, glm::mat4(1));
}

void Model::imguiDebug() {
  for (auto [name, animation] : animations) {
    if (ImGui::TreeNode(name.c_str())) {
      ImGui::Text("Duration: %f", animation.duration);
      ImGui::Text("Ticks/Second: %f", animation.tps);

      ImGui::TreePop();
    }
  }
}

static CVar r_anim("r_anim", "1");

void Model::calcAnimatorTransforms(aiNode* node, Animator* anim,
                                   glm::mat4 parentTransform) {
  Animation* animation = anim->animation;

  glm::mat4 nodeTransform =
      gfx::ConvertMatrixToGLMFormat(node->mTransformation);
  // glm::identity<glm::mat4>();
  if (animation->boneKeys.find(node->mName.C_Str()) !=
      animation->boneKeys.end()) {
    // Log::printf(LOG_DEBUG, "Found bonekey for %s", node->mName.C_Str());
    BoneKeyframe& k = animation->boneKeys[node->mName.C_Str()];
    if (r_anim.getBool()) nodeTransform = boneTransform(k, anim->currentTime);
  }
  glm::mat4 globalTransform = parentTransform * nodeTransform;

  if (boneInfo.find(node->mName.C_Str()) != boneInfo.end()) {
    gfx::BoneInfo& info = boneInfo[node->mName.C_Str()];
    anim->boneMatrices[info.id] =
        inverseGlobalTransform * globalTransform * info.offset;
  }

  for (int i = 0; i < node->mNumChildren; i++)
    calcAnimatorTransforms(node->mChildren[i], anim, globalTransform);
}

void Model::Animator::upload(gfx::BaseProgram* program) {
  if (boneUniformBuffer) {
    boneUniformBuffer->uploadSub(0, sizeof(boneMatrices), boneMatrices);
    program->setParameter(
        "BoneTransformBlock", gfx::DtBuffer,
        {.buffer = {.slot = 1, .buffer = boneUniformBuffer.get()}});
  } else {
    throw std::runtime_error("Call Animator::initBuffer");
    /*for (int i = 0; i < MODEL_MAX_BONE_TRANSFORMS; i++)
      program->setParameter(std::format("boneTransform[{}]", i), gfx::DtMat4,
      {.matrix4x4 = boneMatrices[i]});*/
  }
  program->bind();
}

Model::Animation* Model::getAnimation(std::string name) {
  if (animations.find(name) != animations.end())
    return &animations[name];
  else {
    Log::printf(LOG_WARN, "Unknown animation %s for model %s", name.c_str(),
                getName().c_str());
    return NULL;
  }
}

int Model::positionIdx(BoneKeyframe k, float t) const {
  for (int i = 0; i < k.translations.size() - 1; ++i)
    if (t < k.translations[i + 1].timestamp) return i;
  throw std::runtime_error("");
}

int Model::rotationIdx(BoneKeyframe k, float t) const {
  for (int i = 0; i < k.rotations.size() - 1; ++i)
    if (t < k.rotations[i + 1].timestamp) return i;
  throw std::runtime_error("");
}

int Model::scaleIdx(BoneKeyframe k, float t) const {
  for (int i = 0; i < k.scales.size() - 1; ++i)
    if (t < k.scales[i + 1].timestamp) return i;
  throw std::runtime_error("");
}

static inline float scale_factor(float last_t, float next_t, float t) {
  return (t - last_t) / (next_t - last_t);
}

glm::mat4 Model::interpPosition(BoneKeyframe k, float t) const {
  if (k.translations.size() == 1) {
    return glm::translate(glm::identity<glm::mat4>(),
                          k.translations[0].position);
  }

  int p0i = positionIdx(k, t);
  int p1i = p0i + 1;
  float fac = scale_factor(k.translations[p0i].timestamp,
                           k.translations[p1i].timestamp, t);
  glm::vec3 position =
      glm::mix(k.translations[p0i].position, k.translations[p1i].position, fac);
  return glm::translate(glm::identity<glm::mat4>(), position);
}

glm::mat4 Model::interpRotation(BoneKeyframe k, float t) const {
  if (k.rotations.size() == 1) {
    return glm::toMat4(glm::normalize(k.rotations[0].quat));
  }

  int p0i = rotationIdx(k, t);
  int p1i = p0i + 1;
  float fac =
      scale_factor(k.rotations[p0i].timestamp, k.rotations[p1i].timestamp, t);
  glm::quat rotation =
      glm::slerp(k.rotations[p0i].quat, k.rotations[p1i].quat, fac);
  return glm::toMat4(glm::normalize(rotation));
}

glm::mat4 Model::interpScale(BoneKeyframe k, float t) const {
  if (k.scales.size() == 1) {
    return glm::scale(glm::identity<glm::mat4>(), k.scales[0].scale);
  }

  int p0i = scaleIdx(k, t);
  int p1i = p0i + 1;
  float fac = scale_factor(k.scales[p0i].timestamp, k.scales[p1i].timestamp, t);
  glm::vec3 size = glm::mix(k.scales[p0i].scale, k.scales[p1i].scale, fac);
  return glm::scale(glm::identity<glm::mat4>(), size);
}

glm::mat4 Model::boneTransform(BoneKeyframe k, float t) const {
  return interpPosition(k, t) * interpRotation(k, t) * interpScale(k, t);
}
}  // namespace rdm::resource
