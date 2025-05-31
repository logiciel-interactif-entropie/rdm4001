#include "resource.hpp"

#include <assimp/anim.h>
#include <assimp/material.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/quaternion.h>
#include <assimp/scene.h>
#include <assimp/texture.h>
#include <assimp/types.h>
#include <assimp/vector3.h>
#include <sys/types.h>

#include <algorithm>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <filesystem>
#include <glm/ext/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <stdexcept>

#include "filesystem.hpp"
#include "gfx/base_device.hpp"
#include "gfx/base_types.hpp"
#include "gfx/engine.hpp"
#include "gfx/imgui/imgui.h"
#include "gfx/mesh.hpp"
#include "gfx/stb_image.h"
#include "logging.hpp"
#include "settings.hpp"
namespace rdm {
static resource::Texture* missingTexture;

ResourceManager::ResourceManager() {
  if (!missingTexture)
    missingTexture = dynamic_cast<resource::Texture*>(
        load(BaseResource::Texture, "dat5/missingtexture.png"));
}

void BaseResource::loadData() {
  common::OptionalData data =
      common::FileSystem::singleton()->readFile(name.c_str());
  if (data) {
    onLoadData(data);
    isDataReady = true;
    Log::printf(LOG_DEBUG, "Loaded resource data %s", name.c_str());
  } else {
    Log::printf(LOG_ERROR, "Could not load resource path %s", name.c_str());
  }
  needsData = false;
}

static CVar rsc_load_quota("rsc_load_quota", "100", CVARF_SAVE | CVARF_GLOBAL);

void ResourceManager::tick() {
  int quota_amt = 0;
  for (auto& [rscId, rsc] : resources) {
    if (rsc->getNeedsData()) {
      rsc->loadData();
      quota_amt++;

      if (quota_amt > rsc_load_quota.getInt()) break;
    }
  }
}

BaseResource* ResourceManager::load(BaseResource::Type type,
                                    const char* resourceName) {
  if (BaseResource* rsc = getResource(resourceName)) return rsc;

  if (auto io = common::FileSystem::singleton()->getFileIO(resourceName, "r")) {
    BaseResource* rsc;
    switch (type) {
      case BaseResource::Texture:
        rsc = new resource::Texture(this, resourceName);
        break;
      case BaseResource::Model:
        rsc = new resource::Model(this, resourceName);
        break;
      default:
        throw std::runtime_error("");
    }
    resources[resourceName].reset(rsc);
    delete io.value();
    return rsc;
  } else
    return NULL;
}

static CVar r_upload_quota("r_upload_quota", "100", CVARF_SAVE | CVARF_GLOBAL);

void ResourceManager::tickGfx(gfx::Engine* engine) {
  int quota_amt = 0;
  for (auto& [rscId, rsc] : resources) {
    if (resource::BaseGfxResource* rscg =
            dynamic_cast<resource::BaseGfxResource*>(rsc.get())) {
      if (!rscg->getReady() && rscg->getDataReady()) {
        Log::printf(LOG_DEBUG, "Loaded gfx resource for %s",
                    rscg->getName().c_str());
        rscg->gfxUpload(engine);
        quota_amt++;

        if (quota_amt > r_upload_quota.getInt()) {
          break;
        }
      }
    }
  }
}

void ResourceManager::imgui(gfx::Engine* engine) {
  ImGui::Begin("ResourceManager");
  ImGui::BeginTabBar("Resources");
  for (auto& [id, resource] : resources) {
    ImGui::PushID(id.c_str());
    if (ImGui::BeginTabItem(id.c_str())) {
      ImGui::Text("Type %i, DR: %s", resource->getType(),
                  resource->getDataReady() ? "true" : "false");
      if (resource::BaseGfxResource* gfxr =
              dynamic_cast<resource::BaseGfxResource*>(resource.get())) {
        ImGui::Text("R: %s", gfxr->getReady() ? "true" : "false");
        if (resource::Texture* texture =
                dynamic_cast<resource::Texture*>(resource.get())) {
          ImGui::Image(texture->getTexture()->getImTextureId(),
                       ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
        } else if (resource::Model* model =
                       dynamic_cast<resource::Model*>(resource.get())) {
        }
      }
      ImGui::EndTabItem();
    }
    ImGui::PopID();
  }
  ImGui::EndTabBar();
  ImGui::End();
}

void ResourceManager::deleteGfxResources() {
  for (auto& [rscId, rsc] : resources) {
    if (resource::BaseGfxResource* rscg =
            dynamic_cast<resource::BaseGfxResource*>(rsc.get())) {
      rscg->gfxDelete();
    }
  }
}

namespace resource {
BaseGfxResource::BaseGfxResource(ResourceManager* manager, std::string name)
    : BaseResource(manager, name) {
  isReady = false;
}

Texture::Texture(ResourceManager* manager, std::string name)
    : BaseGfxResource(manager, name) {}

void Texture::gfxDelete() {
  std::scoped_lock l(m);
  texture.reset();
}

void Texture::gfxUpload(gfx::Engine* engine) {
  std::scoped_lock l(m);

  gfx::BaseTexture::Format fmt;
  switch (channels) {
    case 3:
      fmt = gfx::BaseTexture::RGB;
      break;
    case 4:
      fmt = gfx::BaseTexture::RGBA;
      break;
    default:
      return;
  }
  texture = engine->getDevice()->createTexture();
  texture->upload2d(width, height, gfx::DtUnsignedByte, fmt, textureData, 4);
  setReady();
}

void Texture::onLoadData(common::OptionalData data) {
  std::scoped_lock l(m);

  stbi_set_flip_vertically_on_load(true);
  stbi_uc* uc = stbi_load_from_memory(data->data(), data->size(), &width,
                                      &height, &channels, 0);
  textureData = (void*)uc;
}

gfx::BaseTexture* Texture::getTexture() {
  std::scoped_lock l(m);

  if (texture) {
    m.unlock();
    return texture.get();
  } else {
    if (!getDataReady()) {
      setNeedsData();
    }
    if (this == missingTexture)
      return NULL;
    else
      return missingTexture->getTexture();
  }
}

Model::Model(ResourceManager* rm, std::string name)
    : BaseGfxResource(rm, name) {
  path = std::string(name).find_last_of('/');
  broken = true;
  boneCount = 0;
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
 public:
  virtual bool Exists(const char* file) const {
    return common::FileSystem::singleton()->getFileIO(file, "r").has_value();
  }

  virtual char getOsSeparator() const { return '/'; }

  virtual Assimp::IOStream* Open(const char* file, const char* mode) {
    auto io = common::FileSystem::singleton()->getFileIO(file, mode);
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

  for (int mesh_id = 0; mesh_id < scene->mNumMeshes; mesh_id++) {
    aiMesh* mesh = scene->mMeshes[mesh_id];
    gfx::Mesh meshData;
    meshData.skinned = mesh->HasBones();
    meshData.element = engine->getDevice()->createBuffer();
    meshData.vertex = engine->getDevice()->createBuffer();
    meshData.arrayPointers = engine->getDevice()->createArrayPointers();

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
                              mesh->mTextureCoords[0][i].y);
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
          bool modifiedVertex = false;
          for (int z = 0; z < MODEL_MAX_WEIGHTS; z++) {
            if (vertices[vertex].boneIds[z] < 0) {
              vertices[vertex].boneIds[z] = boneId;
              vertices[vertex].boneWeights[z] = value;
              modifiedVertex = true;
              break;
            }
          }
          if (!modifiedVertex) {
            Log::printf(LOG_ERROR,
                        "Bone %s tried to add another weight to %i, which is "
                        "at its limit",
                        bone->mName.C_Str(), vertex);
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
                              mesh->mTextureCoords[0][i].y);
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

  setReady();
}

void Model::onLoadData(common::OptionalData data) {
  namespace fs = std::filesystem;
  fs::path path = getName();
  broken = false;
  if (path.extension() == "cqc") {
  } else {
    importer.SetIOHandler(new AssimpIOSystem());
    scene = importer.ReadFileFromMemory(
        data->data(), data->size(), aiProcess_Triangulate | aiProcess_FlipUVs,
        path.extension().c_str());
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
      aiString texturePath;

      if (material->GetTextureCount(aiTextureType_BASE_COLOR)) {
        material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath);
        albedo =
            getResourceManager()->load<resource::Texture>(texturePath.C_Str());
      }
    }
    for (int i = 0; i < scene->mNumMeshes; i++) {
      aiMesh* mesh = scene->mMeshes[i];
      std::string meshName = mesh->mName.C_Str();

      if (mesh->HasBones()) {
        skinned = true;

        Log::printf(LOG_DEBUG, "HI we aare going to load %i Bones",
                    mesh->mNumBones);
        for (int j = 0; j < mesh->mNumBones; j++) {
          aiBone* bone = mesh->mBones[j];
          std::string boneName = bone->mName.C_Str();
          gfx::BoneInfo boneData;
          boneData.id = boneCount++;
          boneData.offset = gfx::ConvertMatrixToGLMFormat(bone->mOffsetMatrix);
          // boneData.offset = glm::identity<glm::mat4>();
          boneInfo[boneName] = boneData;
        }
      }
    }
    if (skinned) {
      for (int i = 0; i < scene->mNumAnimations; i++) {
        aiAnimation* anim = scene->mAnimations[i];
        Animation animData;

        Log::printf(LOG_DEBUG, "Animation %s", anim->mName.C_Str());
        animData.tps = anim->mTicksPerSecond;
        animData.duration = anim->mDuration;
        for (int j = 0; j < anim->mNumChannels; j++) {
          aiNodeAnim* nodeAnim = anim->mChannels[j];

          if (boneInfo.find(nodeAnim->mNodeName.C_Str()) == boneInfo.end()) {
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
      }
    }
  }
}

void Model::render(gfx::BaseDevice* device) {
  if (!getReady()) return;
  {
    std::scoped_lock l(m);
    for (auto& [name, mesh] : meshes) mesh.render(device);
  }
}

void Model::updateAnimator(gfx::Engine* engine, Animator* anim) {
  if (!anim->animation) return;
  anim->currentTime += anim->animation->tps *
                       engine->getRenderJob()->getStats().deltaTime *
                       anim->speed;
  anim->currentTime = fmod(anim->currentTime, anim->animation->duration);
  calcAnimatorTransforms(scene->mRootNode, anim, glm::mat4(1));
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
  for (int i = 0; i < MODEL_MAX_BONE_TRANSFORMS; i++)
    program->setParameter(std::format("boneTransform[{}]", i), gfx::DtMat4,
                          {.matrix4x4 = boneMatrices[i]});
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
};  // namespace resource
};  // namespace rdm
