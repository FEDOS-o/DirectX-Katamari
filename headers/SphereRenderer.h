#pragma once
#include "Game.h"
#include "Core.h"
#include <vector>
#include <random>
#include <cmath>

using namespace DirectX;

class SphereRenderer {
private:
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    ID3D11InputLayout* inputLayout;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11Buffer* constantBuffer;
    UINT indexCount;
    bool initialized;

public:
    SphereRenderer() : vertexBuffer(nullptr), indexBuffer(nullptr),
        inputLayout(nullptr), vertexShader(nullptr),
        pixelShader(nullptr), constantBuffer(nullptr),
        indexCount(0), initialized(false) {
    }

    ~SphereRenderer() { DestroyResources(); }

    void Initialize(Game* game, const Vector4& baseColor, int segments = 32) {
        if (initialized) return;

        CreateSphere(game, segments, baseColor);
        CreateShaders(game);
        CreateConstantBuffer(game);
        initialized = true;
    }

    void Draw(Game* game, const Matrix& world, const Vector4& color,
        const Matrix& view, const Matrix& projection) {
        if (!initialized || !game || !game->Context) {
            return;
        }

        Matrix wvp = world * view * projection;

        struct ExtendedConstantBuffer {
            Matrix worldViewProj;
            Vector4 objectColor;
            float time;
            Vector3 padding;
        };

        ExtendedConstantBuffer cb;
        cb.worldViewProj = wvp.Transpose();
        cb.objectColor = color;
        cb.time = game->TotalTime;
        cb.padding = Vector3(0, 0, 0);

        game->Context->UpdateSubresource(constantBuffer, 0, nullptr, &cb, 0, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        game->Context->IASetInputLayout(inputLayout);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->VSSetConstantBuffers(0, 1, &constantBuffer);
        game->Context->PSSetShader(pixelShader, nullptr, 0);

        game->Context->DrawIndexed(indexCount, 0, 0);
    }

    void DestroyResources() {
        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
        if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
        if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
        if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
        if (constantBuffer) { constantBuffer->Release(); constantBuffer = nullptr; }
        initialized = false;
    }

private:
    void CreateSphere(Game* game, int segments, const Vector4& baseColor) {
        std::vector<Vertex> vertices;
        std::vector<UINT> indices;

        for (int lat = 0; lat <= segments; lat++) {
            float theta = lat * XM_PI / segments;
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            for (int lon = 0; lon <= segments; lon++) {
                float phi = lon * 2 * XM_PI / segments;
                float sinPhi = sin(phi);
                float cosPhi = cos(phi);

                float x = cosPhi * sinTheta;
                float y = cosTheta;
                float z = sinPhi * sinTheta;

                float r = baseColor.x;
                float g = baseColor.y;
                float b = baseColor.z;

                r += sin(theta * 8.0f) * 0.1f;
                g += cos(phi * 8.0f) * 0.1f;
                b += sin((theta + phi) * 8.0f) * 0.1f;

                r = max(0.1f, min(1.0f, r));
                g = max(0.1f, min(1.0f, g));
                b = max(0.1f, min(1.0f, b));

                vertices.push_back({ Vector3(x, y, z), Vector4(r, g, b, 1.0f) });
            }
        }

        for (int lat = 0; lat < segments; lat++) {
            for (int lon = 0; lon < segments; lon++) {
                int first = lat * (segments + 1) + lon;
                int second = first + segments + 1;

                indices.push_back(first);
                indices.push_back(first + 1);
                indices.push_back(second);

                indices.push_back(second);
                indices.push_back(first + 1);
                indices.push_back(second + 1);
            }
        }

        indexCount = (UINT)indices.size();

        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData = { vertices.data() };
        HRESULT hr = game->Device->CreateBuffer(&vertexDesc, &vertexData, &vertexBuffer);
        if (FAILED(hr)) {
            OutputDebugStringA("Failed to create vertex buffer\n");
            return;
        }

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(UINT) * indexCount;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData = { indices.data() };
        hr = game->Device->CreateBuffer(&indexDesc, &indexData, &indexBuffer);
        if (FAILED(hr)) {
            OutputDebugStringA("Failed to create index buffer\n");
            return;
        }
    }

    void CreateShaders(Game* game) {
        const char* vsCode = R"(
            cbuffer ConstantBuffer : register(b0) {
                float4x4 worldViewProj;
                float4 objectColor;
                float time;
                float3 padding;
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
    
                // Анимируем цвет на основе времени и позиции
                float noise = sin(input.position.x * 5.0f + time) * 
                              cos(input.position.y * 5.0f + time * 1.3f) * 
                              sin(input.position.z * 5.0f + time * 0.7f);
                noise = noise * 0.01f + 0.7f;
    
                output.color = input.color * noise;
                output.color.a = 1.0f;
    
                return output;
            }
        )";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* error = nullptr;

        HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "VSMain", "vs_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, &error);

        if (FAILED(hr)) {
            if (error) {
                OutputDebugStringA((char*)error->GetBufferPointer());
                error->Release();
            }
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
        hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "PSMain", "ps_5_0",
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, &error);

        if (FAILED(hr)) {
            if (error) {
                OutputDebugStringA((char*)error->GetBufferPointer());
                error->Release();
            }
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

    void CreateConstantBuffer(Game* game) {
        struct ExtendedConstantBuffer {
            Matrix worldViewProj;
            Vector4 objectColor;
            float time;
            Vector3 padding;
        };

        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.ByteWidth = sizeof(ExtendedConstantBuffer);
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        game->Device->CreateBuffer(&desc, nullptr, &constantBuffer);
    }
};