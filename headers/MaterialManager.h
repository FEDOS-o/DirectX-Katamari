#pragma once
#include "ModelTypes.h"
#include "TextureLoader.h"
#include <assimp/scene.h>
#include <assimp/material.h>
#include <unordered_map>

class MaterialManager {
private:
    std::vector<MaterialData> materials;
    std::unordered_map<std::string, TextureData> textures;
    Game* game;

    void LoadTexture(const std::string& path) {
        auto it = textures.find(path);
        if (it != textures.end() && it->second.loaded) return;

        if (!game || !game->Device) return;

        TextureData texture;
        texture.path = path;
        texture.textureView = Core::TextureLoader::LoadTexture2D(game, path, true);
        texture.loaded = (texture.textureView != nullptr);

        if (texture.loaded) {
            textures[path] = std::move(texture);
        }
    }

public:
    MaterialManager(Game* inGame) : game(inGame) {}

    ~MaterialManager() {
        Clear();
    }

    // Запрещаем копирование
    MaterialManager(const MaterialManager&) = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;

    void ProcessMaterials(const aiScene* scene, const std::string& modelFolder) {
        if (!game || !scene || scene->mNumMaterials == 0) return;

        materials.resize(scene->mNumMaterials);

        for (UINT i = 0; i < scene->mNumMaterials; i++) {
            aiMaterial* material = scene->mMaterials[i];
            aiColor4D color;

            if (material->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
                materials[i].diffuseColor = Vector4(color.r, color.g, color.b, color.a);
            }
            if (material->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
                materials[i].specularColor = Vector4(color.r, color.g, color.b, color.a);
            }
            if (material->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
                materials[i].ambientColor = Vector4(color.r, color.g, color.b, color.a);
            }

            float shininess = 0.0f;
            if (material->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS) {
                materials[i].shininess = shininess;
            }

            aiString texturePath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
                std::string fullPath = modelFolder + "/" + texturePath.C_Str();
                materials[i].diffuseTexturePath = fullPath;
                materials[i].hasTexture = true;

                if (textures.find(fullPath) == textures.end()) {
                    LoadTexture(fullPath);
                }
            }
        }
    }

    void Clear() {
        materials.clear();
        for (auto& pair : textures) {
            pair.second.Release();
        }
        textures.clear();
    }

    const MaterialData& GetMaterial(int index) const {
        static MaterialData defaultMaterial;
        return (index >= 0 && index < (int)materials.size()) ? materials[index] : defaultMaterial;
    }

    ID3D11ShaderResourceView* GetTexture(const std::string& path) const {
        auto it = textures.find(path);
        return (it != textures.end() && it->second.loaded) ? it->second.textureView : nullptr;
    }

    bool HasTexture(const std::string& path) const {
        auto it = textures.find(path);
        return it != textures.end() && it->second.loaded;
    }

    const std::vector<MaterialData>& GetMaterials() const { return materials; }
};