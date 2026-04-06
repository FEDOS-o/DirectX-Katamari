#pragma once
#include "GameComponent.h"
#include "Camera.h"
#include <SimpleMath.h>
#include <vector>

using namespace DirectX::SimpleMath;

class InfiniteGridGameComponent : public GameComponent {
private:
    struct Vertex {
        Vector3 position;
        Vector4 color;
    };

    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    ID3D11InputLayout* inputLayout;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11Buffer* constantBuffer;

    float baseSpacing;
    int baseVisibleLines;
    UINT indexCount;
    bool initialized;

    Vector3 lastCameraPos;
    float lastCameraHeight;

public:
    InfiniteGridGameComponent(Game* game, float baseSpacing = 1.0f, int baseVisibleLines = 100)
        : GameComponent(game)
        , baseSpacing(baseSpacing)
        , baseVisibleLines(baseVisibleLines)
        , vertexBuffer(nullptr)
        , indexBuffer(nullptr)
        , inputLayout(nullptr)
        , vertexShader(nullptr)
        , pixelShader(nullptr)
        , constantBuffer(nullptr)
        , indexCount(0)
        , initialized(false)
        , lastCameraPos(0, 0, 0)
        , lastCameraHeight(0)
    {
    }

    void Initialize() override {
        if (initialized) return;

        CreateShaders();
        CreateConstantBuffer();
        BuildGrid();

        initialized = true;
    }

    void Update(float deltaTime) override {
        if (!game || !game->Camera) return;

        Vector3 cameraPos = game->Camera->GetPosition();

        float dx = abs(cameraPos.x - lastCameraPos.x);
        float dz = abs(cameraPos.z - lastCameraPos.z);
        float dh = abs(cameraPos.y - lastCameraHeight);

        if (dx > baseSpacing || dz > baseSpacing || dh > 1.0f || !vertexBuffer) {
            lastCameraPos = cameraPos;
            lastCameraHeight = cameraPos.y;
            BuildGrid();
        }
    }

    void Draw() override {
        if (!initialized || !game || !game->Context || !game->Camera || !vertexBuffer) {
            return;
        }

        Matrix view = game->Camera->GetViewMatrix();
        Matrix proj = game->Camera->GetProjectionMatrix();
        Matrix wvp = Matrix::Identity * view * proj;

        struct ConstantBufferData {
            Matrix worldViewProj;
        };

        ConstantBufferData cb;
        cb.worldViewProj = wvp.Transpose();

        game->Context->UpdateSubresource(constantBuffer, 0, nullptr, &cb, 0, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        game->Context->IASetInputLayout(inputLayout);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->VSSetConstantBuffers(0, 1, &constantBuffer);
        game->Context->PSSetShader(pixelShader, nullptr, 0);

        game->Context->DrawIndexed(indexCount, 0, 0);
    }

    void DestroyResources() override {
        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
        if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
        if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
        if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
        if (constantBuffer) { constantBuffer->Release(); constantBuffer = nullptr; }
        initialized = false;
    }

private:
    void BuildGrid() {
        if (!game || !game->Camera) return;

        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }

        std::vector<Vertex> vertices;
        std::vector<UINT> indices;

        Vector3 cameraPos = game->Camera->GetPosition();
        float height = cameraPos.y;

        float spacing = baseSpacing + height * 0.5f;
        if (spacing < baseSpacing) spacing = baseSpacing;

        int visibleLines = baseVisibleLines;

        float centerX = floor(cameraPos.x / spacing) * spacing;
        float centerZ = floor(cameraPos.z / spacing) * spacing;

        float halfSize = visibleLines * spacing;
        float startX = centerX - halfSize;
        float endX = centerX + halfSize;
        float startZ = centerZ - halfSize;
        float endZ = centerZ + halfSize;

        for (int i = -visibleLines; i <= visibleLines; i++) {
            float posX = centerX + i * spacing;
            float posZ = centerZ + i * spacing;

            bool isMajor = (abs(i) % 5 == 0);
            float brightness = isMajor ? 0.7f : 0.35f;
            Vector4 color = Vector4(brightness, brightness, brightness, 1.0f);

            vertices.push_back({ Vector3(startX, 0, posZ), color });
            vertices.push_back({ Vector3(endX, 0, posZ), color });
            indices.push_back(vertices.size() - 2);
            indices.push_back(vertices.size() - 1);

            vertices.push_back({ Vector3(posX, 0, startZ), color });
            vertices.push_back({ Vector3(posX, 0, endZ), color });
            indices.push_back(vertices.size() - 2);
            indices.push_back(vertices.size() - 1);
        }

        vertices.push_back({ Vector3(startX, 0, 0), Vector4(1.0f, 0.2f, 0.2f, 1.0f) });
        vertices.push_back({ Vector3(endX, 0, 0), Vector4(1.0f, 0.2f, 0.2f, 1.0f) });
        indices.push_back(vertices.size() - 2);
        indices.push_back(vertices.size() - 1);

        vertices.push_back({ Vector3(0, 0, startZ), Vector4(0.2f, 0.2f, 1.0f, 1.0f) });
        vertices.push_back({ Vector3(0, 0, endZ), Vector4(0.2f, 0.2f, 1.0f, 1.0f) });
        indices.push_back(vertices.size() - 2);
        indices.push_back(vertices.size() - 1);

        indexCount = (UINT)indices.size();

        if (indexCount == 0) return;

        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData = { vertices.data() };
        HRESULT hr = game->Device->CreateBuffer(&vertexDesc, &vertexData, &vertexBuffer);
        if (FAILED(hr)) return;

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(UINT) * indexCount;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData = { indices.data() };
        game->Device->CreateBuffer(&indexDesc, &indexData, &indexBuffer);
    }

    void CreateShaders() {
        const char* vsCode = R"(
            cbuffer ConstantBuffer : register(b0) {
                float4x4 worldViewProj;
            };
            
            struct VSInput {
                float3 position : POSITION;
                float4 color : COLOR;
            };
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
            };
            
            VSOutput VSMain(VSInput input) {
                VSOutput output;
                output.position = mul(float4(input.position, 1.0f), worldViewProj);
                output.color = input.color;
                return output;
            }
        )";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* error = nullptr;

        HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
            "VSMain", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vsBlob, &error);

        if (FAILED(hr)) {
            if (error) error->Release();
            return;
        }

        const char* psCode = R"(
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
            };
            
            float4 PSMain(VSOutput input) : SV_TARGET {
                return input.color;
            }
        )";

        ID3DBlob* psBlob = nullptr;
        hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr,
            "PSMain", "ps_5_0", D3DCOMPILE_DEBUG, 0, &psBlob, &error);

        if (FAILED(hr)) {
            if (error) error->Release();
            vsBlob->Release();
            return;
        }

        game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

        D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        game->Device->CreateInputLayout(elements, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

        vsBlob->Release();
        psBlob->Release();
        if (error) error->Release();
    }

    void CreateConstantBuffer() {
        struct ConstantBufferData {
            Matrix worldViewProj;
        };

        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = sizeof(ConstantBufferData);
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        game->Device->CreateBuffer(&desc, nullptr, &constantBuffer);
    }
};