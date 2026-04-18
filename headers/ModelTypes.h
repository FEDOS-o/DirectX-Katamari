#pragma once
#include "Core.h"
#include <vector>
#include <string>
#include <d3d11.h>

#pragma warning(push)
#pragma warning(disable: 26495)

// Данные текстуры
struct TextureData {
    ID3D11ShaderResourceView* textureView = nullptr;
    std::string path;
    bool loaded = false;
    int width = 0;
    int height = 0;

    TextureData() = default;

    ~TextureData() {
        Release();
    }

    TextureData(const TextureData&) = delete;
    TextureData& operator=(const TextureData&) = delete;

    TextureData(TextureData&& other) noexcept
        : textureView(other.textureView)
        , path(std::move(other.path))
        , loaded(other.loaded)
        , width(other.width)
        , height(other.height) {
        other.textureView = nullptr;
        other.loaded = false;
    }

    TextureData& operator=(TextureData&& other) noexcept {
        if (this != &other) {
            Release();
            textureView = other.textureView;
            path = std::move(other.path);
            loaded = other.loaded;
            width = other.width;
            height = other.height;
            other.textureView = nullptr;
            other.loaded = false;
        }
        return *this;
    }

    void Release() {
        if (textureView) {
            textureView->Release();
            textureView = nullptr;
        }
        loaded = false;
    }
};

// Данные материала
struct MaterialData {
    DirectX::SimpleMath::Vector4 diffuseColor = { 1, 1, 1, 1 };
    DirectX::SimpleMath::Vector4 specularColor = { 0.5f, 0.5f, 0.5f, 1 };
    DirectX::SimpleMath::Vector4 ambientColor = { 0.2f, 0.2f, 0.2f, 1 };
    float shininess = 32.0f;
    std::string diffuseTexturePath;
    bool hasTexture = false;
};

// Данные меша
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    UINT indexCount = 0;
    int materialIndex = -1;

    MeshData() = default;

    ~MeshData() {
        Release();
    }

    MeshData(const MeshData&) = delete;
    MeshData& operator=(const MeshData&) = delete;

    MeshData(MeshData&& other) noexcept
        : vertices(std::move(other.vertices))
        , indices(std::move(other.indices))
        , vertexBuffer(other.vertexBuffer)
        , indexBuffer(other.indexBuffer)
        , indexCount(other.indexCount)
        , materialIndex(other.materialIndex) {
        other.vertexBuffer = nullptr;
        other.indexBuffer = nullptr;
    }

    MeshData& operator=(MeshData&& other) noexcept {
        if (this != &other) {
            Release();
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
            vertexBuffer = other.vertexBuffer;
            indexBuffer = other.indexBuffer;
            indexCount = other.indexCount;
            materialIndex = other.materialIndex;
            other.vertexBuffer = nullptr;
            other.indexBuffer = nullptr;
        }
        return *this;
    }

    void Release() {
        if (vertexBuffer) {
            vertexBuffer->Release();
            vertexBuffer = nullptr;
        }
        if (indexBuffer) {
            indexBuffer->Release();
            indexBuffer = nullptr;
        }
    }
};

#pragma warning(pop)