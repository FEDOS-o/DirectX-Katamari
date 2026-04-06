#pragma once

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Game.h"
#include "Core.h"
#include "Camera.h"

#include <algorithm>  // днаюбхрэ щрн!
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <wincodec.h>
#include <comdef.h>
#include <DirectXCollision.h>

#pragma comment(lib, "windowscodecs.lib")


using namespace DirectX::SimpleMath;

class Model3D {
private:
    struct TextureData {
        ID3D11ShaderResourceView* textureView;
        std::string path;
        bool loaded;
        int width;
        int height;

        TextureData() : textureView(nullptr), loaded(false), width(0), height(0) {}

        void Release() {
            if (textureView) {
                textureView->Release();
                textureView = nullptr;
            }
        }

        ~TextureData() {
            Release();
        }

        TextureData(const TextureData&) = delete;
        TextureData& operator=(const TextureData&) = delete;

        TextureData(TextureData&& other) noexcept
            : textureView(other.textureView), path(std::move(other.path)),
            loaded(other.loaded), width(other.width), height(other.height) {
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
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<UINT> indices;
        ID3D11Buffer* vertexBuffer;
        ID3D11Buffer* indexBuffer;
        UINT indexCount;
        int materialIndex;

        MeshData() : vertexBuffer(nullptr), indexBuffer(nullptr), indexCount(0), materialIndex(-1) {}
        ~MeshData() {
            if (vertexBuffer) {
                vertexBuffer->Release();
                vertexBuffer = nullptr;
            }
            if (indexBuffer) {
                indexBuffer->Release();
                indexBuffer = nullptr;
            }
        }

        MeshData(const MeshData&) = delete;
        MeshData& operator=(const MeshData&) = delete;
    };

    struct MaterialData {
        Vector4 diffuseColor;
        Vector4 specularColor;
        Vector4 ambientColor;
        float shininess;
        std::string diffuseTexturePath;
        bool hasTexture;

        MaterialData() : diffuseColor(1, 1, 1, 1), specularColor(1, 1, 1, 1),
            ambientColor(0.2f, 0.2f, 0.2f, 1), shininess(32.0f), hasTexture(false) {
        }
    };

    Game* game;
    std::vector<MeshData*> meshes;
    std::vector<MaterialData> materials;
    std::unordered_map<std::string, TextureData> textures;
    std::string modelPath;
    bool isValid;

    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    Matrix worldMatrix;
    bool worldMatrixDirty;

    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11InputLayout* inputLayout;
    ID3D11Buffer* constantBuffer;
    ID3D11SamplerState* samplerState;
    bool shadersInitialized;

    DirectX::BoundingBox localBoundingBox;
    bool boundingBoxCalculated;

    struct ConstantBufferData {
        Matrix worldViewProj;
        int useTexture;
        float padding[3];
    };

    void UpdateWorldMatrix() {
        worldMatrix = Matrix::CreateScale(scale) *
            Matrix::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z) *
            Matrix::CreateTranslation(position);
        worldMatrixDirty = false;
    }

    void ProcessNode(aiNode* node, const aiScene* scene) {
        for (UINT i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            ProcessMesh(mesh, scene);
        }
        for (UINT i = 0; i < node->mNumChildren; i++) {
            ProcessNode(node->mChildren[i], scene);
        }
    }

    void ProcessMesh(aiMesh* mesh, const aiScene* scene) {
        MeshData* meshData = new MeshData();
        meshData->materialIndex = mesh->mMaterialIndex;

        for (UINT i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            memset(&vertex, 0, sizeof(Vertex));

            vertex.position.x = mesh->mVertices[i].x;
            vertex.position.y = mesh->mVertices[i].y;
            vertex.position.z = mesh->mVertices[i].z;

            if (mesh->mTextureCoords[0]) {
                vertex.texCoord.x = mesh->mTextureCoords[0][i].x;
                vertex.texCoord.y = mesh->mTextureCoords[0][i].y;
            }
            else {
                vertex.texCoord = Vector2(0, 0);
            }

            if (mesh->HasVertexColors(0)) {
                vertex.color.x = mesh->mColors[0][i].r;
                vertex.color.y = mesh->mColors[0][i].g;
                vertex.color.z = mesh->mColors[0][i].b;
                vertex.color.w = mesh->mColors[0][i].a;
            }
            else {
                vertex.color = Vector4(1, 1, 1, 1);
            }

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

    void ProcessMaterials(const aiScene* scene, const std::string& modelFolder) {
        if (!game) return;
        if (scene->mNumMaterials == 0) return;

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

            float shininess;
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

    void LoadTexture(const std::string& path) {
        auto it = textures.find(path);
        if (it != textures.end() && it->second.loaded) return;

        if (!game || !game->Device) return;

        std::ifstream file(path);
        if (!file.good()) return;
        file.close();

        TextureData texture;
        texture.path = path;
        texture.loaded = false;
        texture.textureView = nullptr;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

        if (SUCCEEDED(hr)) {
            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            std::wstring wpath(path.begin(), path.end());
            hr = wicFactory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

            if (SUCCEEDED(hr)) {
                Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
                hr = decoder->GetFrame(0, &frame);

                if (SUCCEEDED(hr)) {
                    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
                    hr = wicFactory->CreateFormatConverter(&converter);

                    if (SUCCEEDED(hr)) {
                        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);

                        if (SUCCEEDED(hr)) {
                            UINT width, height;
                            converter->GetSize(&width, &height);
                            texture.width = width;
                            texture.height = height;

                            std::vector<BYTE> pixels(width * height * 4);
                            hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());

                            if (SUCCEEDED(hr)) {
                                D3D11_TEXTURE2D_DESC texDesc = {};
                                texDesc.Width = width;
                                texDesc.Height = height;
                                texDesc.MipLevels = 1;
                                texDesc.ArraySize = 1;
                                texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                                texDesc.SampleDesc.Count = 1;
                                texDesc.SampleDesc.Quality = 0;
                                texDesc.Usage = D3D11_USAGE_DEFAULT;
                                texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                                texDesc.CPUAccessFlags = 0;
                                texDesc.MiscFlags = 0;

                                D3D11_SUBRESOURCE_DATA initData = {};
                                initData.pSysMem = pixels.data();
                                initData.SysMemPitch = width * 4;

                                ID3D11Texture2D* tex = nullptr;
                                hr = game->Device->CreateTexture2D(&texDesc, &initData, &tex);

                                if (SUCCEEDED(hr) && tex) {
                                    hr = game->Device->CreateShaderResourceView(tex, nullptr, &texture.textureView);
                                    tex->Release();

                                    if (SUCCEEDED(hr) && texture.textureView) {
                                        texture.loaded = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        textures[path] = std::move(texture);
        CoUninitialize();
    }

    void CreateBuffers(MeshData* meshData) {
        if (!game || !game->Device || !meshData) return;

        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = (UINT)(sizeof(Vertex) * meshData->vertices.size());
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData = { meshData->vertices.data() };
        game->Device->CreateBuffer(&vertexDesc, &vertexData, &meshData->vertexBuffer);

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = (UINT)(sizeof(UINT) * meshData->indexCount);
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData = { meshData->indices.data() };
        game->Device->CreateBuffer(&indexDesc, &indexData, &meshData->indexBuffer);
    }

    void InitializeShaders() {
        if (shadersInitialized) return;

        const char* vsCode = R"(
            cbuffer ConstantBuffer : register(b0) {
                float4x4 worldViewProj;
                int useTexture;
                float3 padding;
            };
            
            struct VSInput {
                float3 position : POSITION;
                float4 color : COLOR;
                float2 texCoord : TEXCOORD;
            };
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
                float2 texCoord : TEXCOORD0;
            };
            
            VSOutput VSMain(VSInput input) {
                VSOutput output;
                output.position = mul(float4(input.position, 1.0f), worldViewProj);
                output.color = input.color;
                output.texCoord = input.texCoord;
                return output;
            }
        )";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* error = nullptr;

        HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
            "VSMain", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0, &vsBlob, &error);

        if (FAILED(hr)) {
            if (error) error->Release();
            return;
        }

        const char* psCode = R"(
            Texture2D objTexture : register(t0);
            SamplerState objSampler : register(s0);
            
            cbuffer ConstantBuffer : register(b0) {
                float4x4 worldViewProj;
                int useTexture;
                float3 padding;
            };
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
                float2 texCoord : TEXCOORD0;
            };
            
            float4 PSMain(VSOutput input) : SV_TARGET {
                float4 result = input.color;
                if (useTexture != 0) {
                    float4 texColor = objTexture.Sample(objSampler, input.texCoord);
                    result = texColor;
                }
                return result;
            }
        )";

        ID3DBlob* psBlob = nullptr;
        hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr,
            "PSMain", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0, &psBlob, &error);

        if (FAILED(hr)) {
            if (error) error->Release();
            vsBlob->Release();
            return;
        }

        game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

        D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        game->Device->CreateInputLayout(elements, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(ConstantBufferData);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        game->Device->CreateBuffer(&bufferDesc, nullptr, &constantBuffer);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        game->Device->CreateSamplerState(&samplerDesc, &samplerState);

        vsBlob->Release();
        psBlob->Release();
        if (error) error->Release();

        shadersInitialized = true;
    }

public:
    Model3D(Game* inGame) : game(inGame), isValid(false), position(0, 0, 0), rotation(0, 0, 0),
        scale(1, 1, 1), worldMatrixDirty(true), vertexShader(nullptr),
        pixelShader(nullptr), inputLayout(nullptr), constantBuffer(nullptr),
        samplerState(nullptr), shadersInitialized(false), boundingBoxCalculated(false) {
        localBoundingBox.Center = Vector3(0, 0, 0);
        localBoundingBox.Extents = Vector3(0.5f, 0.5f, 0.5f);
    }

    ~Model3D() {
        Unload();
        if (vertexShader) vertexShader->Release();
        if (pixelShader) pixelShader->Release();
        if (inputLayout) inputLayout->Release();
        if (constantBuffer) constantBuffer->Release();
        if (samplerState) samplerState->Release();
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

        std::cout << "[Model3D] BoundingBox - Center: ("
            << localBoundingBox.Center.x << ", " << localBoundingBox.Center.y << ", " << localBoundingBox.Center.z
            << "), Extents: (" << localBoundingBox.Extents.x << ", "
            << localBoundingBox.Extents.y << ", " << localBoundingBox.Extents.z << ")" << std::endl;
    }

    DirectX::BoundingBox GetLocalBoundingBox() const { return localBoundingBox; }

    bool Load(const std::string& path) {
        if (!game || !game->Device) {
            return false;
        }

        Unload();
        modelPath = path;

        InitializeShaders();

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
            std::cout << "ERROR: Failed to load scene! " << importer.GetErrorString() << std::endl;
            return false;
        }

        std::string modelFolder = path;
        size_t lastSlash = modelFolder.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            modelFolder = modelFolder.substr(0, lastSlash);
        }
        else {
            modelFolder = ".";
        }

        ProcessMaterials(scene, modelFolder);
        ProcessNode(scene->mRootNode, scene);

        for (MeshData* mesh : meshes) {
            CreateBuffers(mesh);
        }

        CalculateLocalBoundingBox();

        isValid = !meshes.empty();
        UpdateWorldMatrix();

        std::cout << "[Model3D] Loaded " << meshes.size() << " meshes" << std::endl;
        return isValid;
    }

    void Unload() {
        for (MeshData* mesh : meshes) {
            delete mesh;
        }
        meshes.clear();
        materials.clear();

        for (auto& pair : textures) {
            pair.second.Release();
        }
        textures.clear();

        isValid = false;
        boundingBoxCalculated = false;
    }

    void Draw() {
        if (!isValid || !game || !game->Context || !game->Camera) return;
        if (!shadersInitialized) return;
        if (!vertexShader || !pixelShader || !inputLayout || !constantBuffer) return;

        if (worldMatrixDirty) {
            UpdateWorldMatrix();
        }

        Matrix view = game->Camera->GetViewMatrix();
        Matrix projection = game->Camera->GetProjectionMatrix();
        Matrix wvp = worldMatrix * view * projection;

        game->Context->IASetInputLayout(inputLayout);
        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->PSSetShader(pixelShader, nullptr, 0);
        game->Context->PSSetSamplers(0, 1, &samplerState);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (MeshData* mesh : meshes) {
            if (!mesh->vertexBuffer || !mesh->indexBuffer) continue;

            ConstantBufferData cb = {};
            cb.worldViewProj = wvp.Transpose();
            cb.useTexture = 0;

            ID3D11ShaderResourceView* currentTexture = nullptr;

            if (mesh->materialIndex >= 0 && mesh->materialIndex < (int)materials.size()) {
                MaterialData& mat = materials[mesh->materialIndex];
                if (mat.hasTexture) {
                    auto it = textures.find(mat.diffuseTexturePath);
                    if (it != textures.end() && it->second.loaded && it->second.textureView) {
                        currentTexture = it->second.textureView;
                        cb.useTexture = 1;
                    }
                }
            }

            game->Context->PSSetShaderResources(0, 1, &currentTexture);
            game->Context->UpdateSubresource(constantBuffer, 0, nullptr, &cb, 0, 0);
            game->Context->VSSetConstantBuffers(0, 1, &constantBuffer);
            game->Context->PSSetConstantBuffers(0, 1, &constantBuffer);

            UINT stride = sizeof(Vertex);
            UINT offset = 0;
            game->Context->IASetVertexBuffers(0, 1, &mesh->vertexBuffer, &stride, &offset);
            game->Context->IASetIndexBuffer(mesh->indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            game->Context->DrawIndexed(mesh->indexCount, 0, 0);
        }
    }

    void SetPosition(const Vector3& pos) {
        position = pos;
        worldMatrixDirty = true;
    }

    void SetRotation(const Vector3& rot) {
        rotation = rot;
        worldMatrixDirty = true;
    }

    void SetScale(const Vector3& scl) {
        scale = scl;
        worldMatrixDirty = true;
    }

    Vector3 GetPosition() const { return position; }
    Vector3 GetRotation() const { return rotation; }
    Vector3 GetScale() const { return scale; }

    bool IsValid() const { return isValid; }
    size_t GetMeshCount() const { return meshes.size(); }
};