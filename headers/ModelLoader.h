#pragma once
#include "ModelTypes.h"
#include "MeshManager.h"
#include "MaterialManager.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <DirectXCollision.h>
#include <float.h>

class ModelLoader {
private:
    Game* game;
    std::vector<MeshData*> meshes;
    DirectX::BoundingBox localBoundingBox;
    bool boundingBoxCalculated = false;

    void ProcessMesh(aiMesh* mesh) {
        MeshData* meshData = new MeshData();
        meshData->materialIndex = mesh->mMaterialIndex;

        for (UINT i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            memset(&vertex, 0, sizeof(Vertex));

            vertex.position = Vector3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

            if (mesh->HasNormals()) {
                vertex.normal = Vector3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
                vertex.normal.Normalize();
            }
            else {
                vertex.normal = Vector3(0, 1, 0);
            }

            vertex.texCoord = mesh->mTextureCoords[0] ?
                Vector2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y) : Vector2(0, 0);

            vertex.color = mesh->HasVertexColors(0) ?
                Vector4(mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a) :
                Vector4(1, 1, 1, 1);

            meshData->vertices.push_back(vertex);
        }

        for (UINT i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            for (UINT j = 0; j < face.mNumIndices; j++) {
                meshData->indices.push_back(face.mIndices[j]);
            }
        }

        meshData->indexCount = (UINT)meshData->indices.size();
        meshes.push_back(meshData);
    }

    void ProcessNode(aiNode* node, const aiScene* scene) {
        for (UINT i = 0; i < node->mNumMeshes; i++) {
            ProcessMesh(scene->mMeshes[node->mMeshes[i]]);
        }
        for (UINT i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene);
        }
    }

public:
    ModelLoader(Game* inGame) : game(inGame) {
        localBoundingBox.Center = Vector3(0, 0, 0);
        localBoundingBox.Extents = Vector3(0.5f, 0.5f, 0.5f);
    }

    ~ModelLoader() {
        Unload();
    }

    // Запрещаем копирование
    ModelLoader(const ModelLoader&) = delete;
    ModelLoader& operator=(const ModelLoader&) = delete;

    bool Load(const std::string& path, MaterialManager& materialManager) {
        if (!game || !game->Device) return false;

        Unload();

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_OptimizeMeshes |
            aiProcess_ImproveCacheLocality |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_FlipUVs);

        if (!scene || !scene->mRootNode) {
            return false;
        }

        std::string modelFolder = path;
        size_t lastSlash = modelFolder.find_last_of("/\\");
        modelFolder = (lastSlash != std::string::npos) ? modelFolder.substr(0, lastSlash) : ".";

        materialManager.ProcessMaterials(scene, modelFolder);
        ProcessNode(scene->mRootNode, scene);

        for (MeshData* mesh : meshes) {
            MeshManager::CreateBuffers(game, mesh);
        }

        return !meshes.empty();
    }

    void Unload() {
        for (MeshData* mesh : meshes) {
            mesh->Release();
            delete mesh;
        }
        meshes.clear();
        boundingBoxCalculated = false;
    }

    void CalculateLocalBoundingBox() {
        if (boundingBoxCalculated) return;

        Vector3 minPos(FLT_MAX, FLT_MAX, FLT_MAX);
        Vector3 maxPos(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (MeshData* mesh : meshes) {
            for (const Vertex& vertex : mesh->vertices) {
                minPos.x = std::min(minPos.x, vertex.position.x);
                minPos.y = std::min(minPos.y, vertex.position.y);
                minPos.z = std::min(minPos.z, vertex.position.z);
                maxPos.x = std::max(maxPos.x, vertex.position.x);
                maxPos.y = std::max(maxPos.y, vertex.position.y);
                maxPos.z = std::max(maxPos.z, vertex.position.z);
            }
        }

        Vector3 size = maxPos - minPos;
        localBoundingBox.Center = (minPos + maxPos) / 2.0f;
        localBoundingBox.Extents = size / 2.0f;
        boundingBoxCalculated = true;
    }

    const std::vector<MeshData*>& GetMeshes() const { return meshes; }
    DirectX::BoundingBox GetLocalBoundingBox() const { return localBoundingBox; }
    bool IsValid() const { return !meshes.empty(); }
};