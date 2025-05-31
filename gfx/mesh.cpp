#include "mesh.hpp"

#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "engine.hpp"
#include "filesystem.hpp"
#include "gfx/base_types.hpp"
#include "logging.hpp"

namespace rdm::gfx {
void Model::process(Engine* engine) {
  this->engine = engine;
  processNode(engine, scene->mRootNode);
}

void Model::processNode(Engine* engine, aiNode* node) {
  for (int i = 0; i < node->mNumMeshes; i++) {
    aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
    meshes.push_back(processMesh(mesh));
  }
  for (int i = 0; i < node->mNumChildren; i++) {
    processNode(engine, node->mChildren[i]);
  }
}

Model::~Model() {
  for (auto texture : textures) {
    engine->getTextureCache()->deleteTexture(texture.c_str());
  }
}

void Mesh::render(BaseDevice* device) {
  arrayPointers->bind();
  device->draw(element.get(), DtUnsignedInt, BaseDevice::Triangles,
               indices.size());
}

void Model::render(BaseDevice* device) {
  for (auto& mesh : meshes) mesh.render(device);
}

// https://learnopengl.com/code_viewer_gh.php?code=includes/learnopengl/assimp_glm_helpers.h
glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from) {
  glm::mat4 to;
  // the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
  to[0][0] = from.a1;
  to[1][0] = from.a2;
  to[2][0] = from.a3;
  to[3][0] = from.a4;
  to[0][1] = from.b1;
  to[1][1] = from.b2;
  to[2][1] = from.b3;
  to[3][1] = from.b4;
  to[0][2] = from.c1;
  to[1][2] = from.c2;
  to[2][2] = from.c3;
  to[3][2] = from.c4;
  to[0][3] = from.d1;
  to[1][3] = from.d2;
  to[2][3] = from.d3;
  to[3][3] = from.d4;
  return to;
}

Mesh Model::processMesh(aiMesh* mesh) {
  Mesh m;

  if (mesh->HasBones()) {
    m.skinned = true;
  }
  for (int i = 0; i < mesh->mNumFaces; i++) {
    aiFace face = mesh->mFaces[i];
    for (int i = 0; i < face.mNumIndices; i++) {
      m.indices.push_back(face.mIndices[i]);
    }
  }
  if (mesh->mMaterialIndex >= 0) {
  }
  m.vertex = engine->getDevice()->createBuffer();
  m.element = engine->getDevice()->createBuffer();

  m.element->upload(BaseBuffer::Element, BaseBuffer::StaticDraw,
                    m.indices.size() * sizeof(unsigned int), m.indices.data());

  m.arrayPointers = engine->getDevice()->createArrayPointers();
  if (m.skinned) {
    std::vector<MeshVertexSkinned> vertices;
    for (int i = 0; i < mesh->mNumVertices; i++) {
      MeshVertexSkinned vertex;
      vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y,
                                  mesh->mVertices[i].z);
      vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y,
                                mesh->mNormals[i].z);
      vertex.uv =
          glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
      // vertex.boneIds = glm::ivec4(-1);
      // vertex.boneWeights = glm::vec4(0.f);
      vertices.push_back(vertex);
    }

    /*
    // extract bone weights
    for (int i = 0; i < mesh->mNumBones; ++i) {
      int boneId = -1;
      std::string boneName = mesh->mBones[i]->mName.C_Str();
      auto it = m.bones.find(boneName);
      if (it != m.bones.end()) {
        BoneInfo info;
        info.id = boneCounter;
        info.offset = ConvertMatrixToGLMFormat(mesh->mBones[i]->mOffsetMatrix);
        m.bones[boneName] = info;
        boneId = boneCounter;
        boneCounter++;
      } else {
        boneId = it->second.id;
      }
      assert(boneId != -1);
      auto weights = mesh->mBones[i]->mWeights;
      int numWeights = mesh->mBones[i]->mNumWeights;
      for (int j = 0; j < numWeights; ++j) {
        int vertex = weights[j].mVertexId;
        float weight = weights[j].mWeight;
        assert(vertex <= vertices.size());
        switch (j) {
          case 0:  // x
            vertices[vertex].boneIds.x = boneId;
            vertices[vertex].boneWeights.x = weight;
            break;
          case 1:  // y
            vertices[vertex].boneIds.y = boneId;
            vertices[vertex].boneWeights.y = weight;
            break;
          case 2:  // z
            vertices[vertex].boneIds.z = boneId;
            vertices[vertex].boneWeights.z = weight;
            break;
          case 3:  // w
            vertices[vertex].boneIds.w = boneId;
            vertices[vertex].boneWeights.w = weight;
            break;
          default:
            break;  // do nothing
        }
        }
        }*/

    m.vertex->upload(BaseBuffer::Array, BaseBuffer::StaticDraw,
                     vertices.size() * sizeof(MeshVertexSkinned),
                     vertices.data());

    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtFloat, 0, 3, sizeof(MeshVertexSkinned),
        (void*)offsetof(MeshVertexSkinned, position), m.vertex.get()));
    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtFloat, 1, 3, sizeof(MeshVertexSkinned),
        (void*)offsetof(MeshVertexSkinned, normal), m.vertex.get()));
    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtFloat, 2, 2, sizeof(MeshVertexSkinned),
        (void*)offsetof(MeshVertexSkinned, uv), m.vertex.get()));
    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtInt, 3, 4, sizeof(MeshVertexSkinned),
        (void*)offsetof(MeshVertexSkinned, boneIds), m.vertex.get()));
    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtFloat, 4, 4, sizeof(MeshVertexSkinned),
        (void*)offsetof(MeshVertexSkinned, boneWeights), m.vertex.get()));

    Log::printf(
        LOG_DEBUG,
        "created skinned mesh with %i vertices, %i bones and %i elements",
        vertices.size(), m.bones.size(), m.indices.size());
  } else {
    std::vector<MeshVertex> vertices;
    for (int i = 0; i < mesh->mNumVertices; i++) {
      MeshVertex vertex;
      vertex.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y,
                                  mesh->mVertices[i].z);
      vertex.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y,
                                mesh->mNormals[i].z);
      vertex.uv =
          glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
      vertices.push_back(vertex);
    }

    m.vertex->upload(BaseBuffer::Array, BaseBuffer::StaticDraw,
                     vertices.size() * sizeof(MeshVertex), vertices.data());

    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtFloat, 0, 3, sizeof(MeshVertex),
        (void*)offsetof(MeshVertex, position), m.vertex.get()));
    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtFloat, 1, 3, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal),
        m.vertex.get()));
    m.arrayPointers->addAttrib(BaseArrayPointers::Attrib(
        DtFloat, 2, 2, sizeof(MeshVertex), (void*)offsetof(MeshVertex, uv),
        m.vertex.get()));

    Log::printf(LOG_DEBUG,
                "created unskinned mesh with %i vertices and %i elements",
                vertices.size(), m.indices.size());
  }
  m.arrayPointers->upload();

  return m;
}

Primitive::Primitive(Type type, Engine* engine) {
  std::vector<MeshVertex> vertices;
  std::vector<unsigned char> indices;
  t = type;
  switch (type) {
    case PlaneZ:
      vertices.push_back(MeshVertex{.position = glm::vec3(0.0, 0.0, 0.0),
                                    .normal = glm::vec3(0.0, 0.0, 1.0),
                                    .uv = glm::vec2(0.0, 0.0)});
      vertices.push_back(MeshVertex{.position = glm::vec3(1.0, 0.0, 0.0),
                                    .normal = glm::vec3(0.0, 0.0, 1.0),
                                    .uv = glm::vec2(1.0, 0.0)});
      vertices.push_back(MeshVertex{.position = glm::vec3(0.0, 1.0, 0.0),
                                    .normal = glm::vec3(0.0, 0.0, 1.0),
                                    .uv = glm::vec2(0.0, 1.0)});
      vertices.push_back(MeshVertex{.position = glm::vec3(1.0, 1.0, 0.0),
                                    .normal = glm::vec3(0.0, 0.0, 1.0),
                                    .uv = glm::vec2(1.0, 1.0)});

      indices.push_back(0);
      indices.push_back(1);
      indices.push_back(2);
      indices.push_back(2);
      indices.push_back(1);
      indices.push_back(3);
      break;
    default:
      Log::printf(LOG_ERROR, "Unknown type %i", type);
      break;
  }
  vertex = engine->getDevice()->createBuffer();
  element = engine->getDevice()->createBuffer();

  vertex->upload(BaseBuffer::Array, BaseBuffer::StaticDraw,
                 vertices.size() * sizeof(MeshVertex), vertices.data());
  element->upload(BaseBuffer::Element, BaseBuffer::StaticDraw,
                  indices.size() * sizeof(unsigned char), indices.data());

  arrayPointers = engine->getDevice()->createArrayPointers();
  arrayPointers->addAttrib(BaseArrayPointers::Attrib(
      DtFloat, 0, 3, sizeof(MeshVertex), (void*)offsetof(MeshVertex, position),
      vertex.get()));
  arrayPointers->addAttrib(BaseArrayPointers::Attrib(
      DtFloat, 1, 3, sizeof(MeshVertex), (void*)offsetof(MeshVertex, normal),
      vertex.get()));
  arrayPointers->addAttrib(
      BaseArrayPointers::Attrib(DtFloat, 2, 2, sizeof(MeshVertex),
                                (void*)offsetof(MeshVertex, uv), vertex.get()));
  arrayPointers->upload();

  indicesCount = indices.size();
}

void Primitive::render(BaseDevice* device) {
  arrayPointers->bind();
  device->draw(element.get(), DtUnsignedByte, BaseDevice::Triangles,
               indicesCount);
}

MeshCache::MeshCache(Engine* engine) {
  this->engine = engine;
  for (int i = 0; i < Primitive::Count; i++) {
    primitives.push_back(Primitive((Primitive::Type)i, engine));
  }
}

Primitive* MeshCache::get(Primitive::Type type) { return &primitives[type]; }

void MeshCache::del(const char* path) { models.erase(path); }

std::optional<Model*> MeshCache::get(const char* path) {
  auto it = models.find(path);
  if (it != models.end()) {
    return models[path].get();
  } else {
    Assimp::Importer importer;
    common::OptionalData data = common::FileSystem::singleton()->readFile(path);
    if (data) {
      Model* model = new Model();
      model->directory = std::string(path).find_last_of('/');
      model->scene = importer.ReadFileFromMemory(
          data.value().data(), data.value().size(),
          aiProcess_Triangulate | aiProcess_FlipUVs);
      if (!model->scene || model->scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
          !model->scene->mRootNode) {
        Log::printf(LOG_ERROR, "Error importing from assimp: %s",
                    importer.GetErrorString());
        return {};
      }

      model->process(engine);
      models[path].reset(model);
      return models[path].get();
    } else {
      Log::printf(LOG_WARN, "Could not open model %s", path);
      return {};
    }
  }
}
}  // namespace rdm::gfx
