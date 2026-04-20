#pragma once
#include "GameComponent.h"
#include "TextureLoader.h"
#include "ShadowRenderer.h"
#include <SimpleMath.h>
#include <vector>
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX::SimpleMath;

class TexturedGround : public GameComponent {
private:
    struct Vertex {
        Vector3 position;
        Vector2 texCoord;
        Vector3 normal;
    };

    // Структуры для константных буферов
    struct MatrixBufferType {
        Matrix world;
        Matrix view;
        Matrix projection;
        Matrix lightView;
        Matrix lightProjection;
    };

    struct LightPositionBufferType {
        Vector3 lightPosition;
        float padding;
    };

    struct LightBufferType {
        Vector4 ambientColor;
        Vector4 diffuseColor;
        float bias;
        float padding[3];
    };

    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11Buffer* matrixBuffer = nullptr;        // VS buffer 0
    ID3D11Buffer* lightPositionBuffer = nullptr; // VS buffer 1
    ID3D11Buffer* lightBuffer = nullptr;         // PS buffer 0
    ID3D11SamplerState* samplerStateWrap = nullptr;
    ID3D11SamplerState* samplerStateClamp = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;

    UINT indexCount = 0;
    bool initialized = false;
    bool textureLoaded = false;
    float size;
    int segments;
    std::string texturePath;

    ID3DBlob* CompileShader(const char* code, const char* target, const char* entryPoint) {
        ID3DBlob* blob = nullptr;
        ID3DBlob* error = nullptr;
        HRESULT hr = D3DCompile(code, strlen(code), nullptr, nullptr, nullptr,
            entryPoint, target, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &blob, &error);

        if (FAILED(hr) && error) {
            OutputDebugStringA((char*)error->GetBufferPointer());
            error->Release();
            return nullptr;
        }
        return blob;
    }

    void CreateGeometry() {
        std::vector<Vertex> vertices;
        std::vector<UINT> indices;

        float halfSize = size / 2.0f;
        float step = size / segments;

        for (int i = 0; i <= segments; i++) {
            float z = -halfSize + i * step;
            float v = (float)i / segments;

            for (int j = 0; j <= segments; j++) {
                float x = -halfSize + j * step;
                float u = (float)j / segments;

                Vertex vert;
                vert.position = Vector3(x, 0.0f, z);
                vert.texCoord = Vector2(u, v);
                vert.normal = Vector3(0, 1, 0);
                vertices.push_back(vert);
            }
        }

        for (int i = 0; i < segments; i++) {
            for (int j = 0; j < segments; j++) {
                int first = i * (segments + 1) + j;
                int second = first + segments + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
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
            std::cout << "Failed to create vertex buffer!" << std::endl;
        }

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(UINT) * indexCount;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA indexData = { indices.data() };

        hr = game->Device->CreateBuffer(&indexDesc, &indexData, &indexBuffer);
        if (FAILED(hr)) {
            std::cout << "Failed to create index buffer!" << std::endl;
        }
    }

    void CreateShaders() {
        // Вершинный шейдер
        const char* vsCode = R"(
            cbuffer MatrixBuffer : register(b0) {
                matrix worldMatrix;
                matrix viewMatrix;
                matrix projectionMatrix;
                matrix lightViewMatrix;
                matrix lightProjectionMatrix;
            }

            cbuffer LightPositionBuffer : register(b1) {
                float3 lightPosition;
                float padding;
            }

            struct VSInput {
                float3 position : POSITION;
                float2 tex : TEXCOORD;
                float3 normal : NORMAL;
            };

            struct VSOutput {
                float4 position : SV_POSITION;
                float2 tex : TEXCOORD0;
                float3 normal : NORMAL;
                float4 lightViewPosition : TEXCOORD1;
                float3 lightPos : TEXCOORD2;
            };

            VSOutput VSMain(VSInput input) {
                VSOutput output;
                float4 worldPosition;
                
                float4 pos = float4(input.position, 1.0f);
                
                output.position = mul(pos, worldMatrix);
                output.position = mul(output.position, viewMatrix);
                output.position = mul(output.position, projectionMatrix);
                
                output.lightViewPosition = mul(pos, worldMatrix);
                output.lightViewPosition = mul(output.lightViewPosition, lightViewMatrix);
                output.lightViewPosition = mul(output.lightViewPosition, lightProjectionMatrix);
                
                output.tex = input.tex;
                
                output.normal = mul(input.normal, (float3x3)worldMatrix);
                output.normal = normalize(output.normal);
                
                worldPosition = mul(pos, worldMatrix);
                
                output.lightPos = lightPosition.xyz - worldPosition.xyz;
                output.lightPos = normalize(output.lightPos);
                
                return output;
            }
        )";

        // Пиксельный шейдер
        const char* psCode = R"(
    Texture2D shaderTexture : register(t0);
    Texture2D depthMapTexture : register(t1);
    SamplerState SampleTypeClamp : register(s0);
    SamplerState SampleTypeWrap : register(s1);

    cbuffer LightBuffer : register(b0) {
        float4 ambientColor;
        float4 diffuseColor;
        float bias;
        float3 padding;
    }

    struct VSOutput {
        float4 position : SV_POSITION;
        float2 tex : TEXCOORD0;
        float3 normal : NORMAL;
        float4 lightViewPosition : TEXCOORD1;
        float3 lightPos : TEXCOORD2;
    };

    float4 PSMain(VSOutput input) : SV_TARGET {
        float4 color = ambientColor;
        float2 projectTexCoord;
        float depthValue;
        float lightDepthValue;
        float lightIntensity;
        float4 textureColor;
        
        projectTexCoord.x = input.lightViewPosition.x / input.lightViewPosition.w / 2.0f + 0.5f;
        projectTexCoord.y = -input.lightViewPosition.y / input.lightViewPosition.w / 2.0f + 0.5f;
        
        // Вычисляем глубину света (расстояние от источника света)
        lightDepthValue = input.lightViewPosition.z / input.lightViewPosition.w;
        
        // Проверяем, что в пределах shadow map
        if (projectTexCoord.x >= 0.0f && projectTexCoord.x <= 1.0f &&
            projectTexCoord.y >= 0.0f && projectTexCoord.y <= 1.0f) {
            
            // Получаем глубину из shadow map
            depthValue = depthMapTexture.Sample(SampleTypeClamp, projectTexCoord).r;
            
            // Сравниваем с учетом bias
            // ВАЖНО: lightDepthValue должно быть МЕНЬШЕ depthValue для освещения
            // bias вычитается из lightDepthValue, делая его "ближе" к свету
            if (lightDepthValue - bias < depthValue) {
                lightIntensity = saturate(dot(input.normal, input.lightPos));
                if (lightIntensity > 0.0f) {
                    color += (diffuseColor * lightIntensity);
                    color = saturate(color);
                }
            }
        } else {
            // За пределами shadow map - считаем освещенным
            lightIntensity = saturate(dot(input.normal, input.lightPos));
            if (lightIntensity > 0.0f) {
                color += (diffuseColor * lightIntensity);
                color = saturate(color);
            }
        }
        
        textureColor = shaderTexture.Sample(SampleTypeWrap, input.tex);
        color = color * textureColor;
        
        return color;
    }
)";

        ID3DBlob* vsBlob = CompileShader(vsCode, "vs_5_0", "VSMain");
        ID3DBlob* psBlob = CompileShader(psCode, "ps_5_0", "PSMain");

        if (!vsBlob || !psBlob) {
            if (vsBlob) vsBlob->Release();
            if (psBlob) psBlob->Release();
            std::cout << "Shader compilation failed!" << std::endl;
            return;
        }

        game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

        D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        game->Device->CreateInputLayout(elements, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

        vsBlob->Release();
        psBlob->Release();
    }

    void CreateBuffers() {
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        desc.ByteWidth = sizeof(MatrixBufferType);
        game->Device->CreateBuffer(&desc, nullptr, &matrixBuffer);

        desc.ByteWidth = sizeof(LightPositionBufferType);
        game->Device->CreateBuffer(&desc, nullptr, &lightPositionBuffer);

        desc.ByteWidth = sizeof(LightBufferType);
        game->Device->CreateBuffer(&desc, nullptr, &lightBuffer);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        game->Device->CreateSamplerState(&samplerDesc, &samplerStateClamp);

        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        game->Device->CreateSamplerState(&samplerDesc, &samplerStateWrap);
    }

public:
    TexturedGround(Game* game, float size = 100.0f, int segments = 100, const std::string& textureFile = "models/wood.jpg")
        : GameComponent(game), size(size), segments(segments), texturePath(textureFile) {
    }

    ~TexturedGround() {
        DestroyResources();
    }

    void Initialize() override {
        if (initialized) return;

        CreateGeometry();
        CreateShaders();
        CreateBuffers();
        textureView = Core::TextureLoader::LoadTexture2D(game, texturePath, true);
        textureLoaded = (textureView != nullptr);

        initialized = true;
    }

    void Update(float deltaTime) override {}

    void Draw() override {
        if (!initialized || !game || !game->Context || !game->Camera) return;
        if (!vertexBuffer || !indexBuffer) return;

        // Заполняем матричный буфер
        MatrixBufferType matBufferData;
        Matrix world = Matrix::Identity;
        matBufferData.world = world.Transpose();
        matBufferData.view = game->Camera->GetViewMatrix().Transpose();
        matBufferData.projection = game->Camera->GetProjectionMatrix().Transpose();
        matBufferData.lightView = game->GetLightViewMatrix().Transpose();
        matBufferData.lightProjection = game->GetLightProjectionMatrix().Transpose();
        game->Context->UpdateSubresource(matrixBuffer, 0, nullptr, &matBufferData, 0, 0);

        // Заполняем буфер позиции света
        LightPositionBufferType lightPosData;
        Vector3 lightPos = Vector3(-20.0f, 30.0f, -20.0f);
        lightPosData.lightPosition = lightPos;
        lightPosData.padding = 0.0f;
        game->Context->UpdateSubresource(lightPositionBuffer, 0, nullptr, &lightPosData, 0, 0);

        // Заполняем буфер света для PS - ЭКСПЕРИМЕНТИРУЕМ С BIAS
        LightBufferType lightPSData;
        lightPSData.ambientColor = Vector4(0.3f, 0.3f, 0.3f, 1.0f);
        lightPSData.diffuseColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

        // ПРОБУЕМ РАЗНЫЕ ЗНАЧЕНИЯ:
        // 0.0001f - слишком мало, тень "прилипает"
        // 0.001f  - нормально для небольших сцен
        // 0.01f   - много, тень "уплывает"
        // 0.05f   - очень много, тень отделяется от объекта
        lightPSData.bias = 0.005f;  // Начните с этого

        lightPSData.padding[0] = lightPSData.padding[1] = lightPSData.padding[2] = 0.0f;
        game->Context->UpdateSubresource(lightBuffer, 0, nullptr, &lightPSData, 0, 0);

        // Устанавливаем вершинный и индексный буферы
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        game->Context->IASetInputLayout(inputLayout);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Устанавливаем шейдеры и константные буферы
        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->VSSetConstantBuffers(0, 1, &matrixBuffer);
        game->Context->VSSetConstantBuffers(1, 1, &lightPositionBuffer);

        game->Context->PSSetShader(pixelShader, nullptr, 0);
        game->Context->PSSetConstantBuffers(0, 1, &lightBuffer);
        game->Context->PSSetSamplers(0, 1, &samplerStateClamp);
        game->Context->PSSetSamplers(1, 1, &samplerStateWrap);

        // Устанавливаем текстуры
        if (textureLoaded && textureView) {
            game->Context->PSSetShaderResources(0, 1, &textureView);
        }

        // Устанавливаем shadow map
        if (game->ShadowMapSRV) {
            game->Context->PSSetShaderResources(1, 1, &game->ShadowMapSRV);
        }

        // Рисуем
        game->Context->DrawIndexed(indexCount, 0, 0);
    }

    void DrawShadow() override {
       /* if (!vertexBuffer || !indexBuffer) {
            return;
        }

        Matrix world = Matrix::Identity;

        if (game && game->ShadowRendererComp) {
            game->ShadowRendererComp->DrawMesh(game,
                vertexBuffer, 0,
                indexBuffer, indexCount, world);
        }*/
    }

    void DestroyResources() override {
        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
        if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
        if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
        if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
        if (matrixBuffer) { matrixBuffer->Release(); matrixBuffer = nullptr; }
        if (lightPositionBuffer) { lightPositionBuffer->Release(); lightPositionBuffer = nullptr; }
        if (lightBuffer) { lightBuffer->Release(); lightBuffer = nullptr; }
        if (samplerStateWrap) { samplerStateWrap->Release(); samplerStateWrap = nullptr; }
        if (samplerStateClamp) { samplerStateClamp->Release(); samplerStateClamp = nullptr; }
        if (textureView) { textureView->Release(); textureView = nullptr; }
        initialized = false;
        textureLoaded = false;
    }
};