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


    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11Buffer* vsConstantBuffer = nullptr;     // VS buffer 0: VSConstantBuffer
    ID3D11Buffer* shadowConstantBuffer = nullptr; // VS buffer 1: ShadowConstantBuffer
    ID3D11Buffer* psConstantBuffer = nullptr;     // PS buffer 0: PSConstantBuffer
    ID3D11Buffer* materialBuffer = nullptr;       // PS buffer 1: MaterialBuffer
    ID3D11Buffer* lightBuffer = nullptr;          // PS buffer 2: DirectionalLightBuffer
    ID3D11SamplerState* samplerState = nullptr;
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
    cbuffer VSConstantBuffer : register(b0) {
        float4x4 world;
        float4x4 view;
        float4x4 projection;
        float4x4 worldInvTranspose;
    }

    cbuffer ShadowConstantBuffer : register(b1) {
        float4x4 lightView;
        float4x4 lightProjection;
    }

    struct VSInput {
        float3 position : POSITION;
        float2 texCoord : TEXCOORD;
        float3 normal : NORMAL;
    };

    struct VSOutput {
        float4 position : SV_POSITION;
        float2 texCoord : TEXCOORD0;
        float3 worldNormal : TEXCOORD1;
        float3 worldPosition : TEXCOORD2;
        float4 shadowPos : TEXCOORD3;
    };

    VSOutput VSMain(VSInput input) {
        VSOutput output;
        
        float4 worldPos = mul(float4(input.position, 1.0f), world);
        output.worldPosition = worldPos.xyz;
        output.position = mul(worldPos, view);
        output.position = mul(output.position, projection);
        
        output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldInvTranspose).xyz);
        output.texCoord = input.texCoord;
        
        float4 lightViewPos = mul(worldPos, lightView);
        output.shadowPos = mul(lightViewPos, lightProjection);
        
        return output;
    }
)";

        // Пиксельный шейдер
        const char* psCode = R"(
    cbuffer PSConstantBuffer : register(b0) {
        float4 cameraPosition;
        float4 objectColor;
        int useTexture;
        int hasMaterial;
        int useReflection;
        int useShadow;
        float shadowBias;
        float padding;
    }

    cbuffer MaterialBuffer : register(b1) {
        float4 materialAmbient;
        float4 materialDiffuse;
        float4 materialSpecular;
        float shininess;
        float3 materialPadding;
    }

    cbuffer DirectionalLightBuffer : register(b2) {
        float4 lightAmbient;
        float4 lightDiffuse;
        float4 lightSpecular;
        float3 lightDirection;
        float lightPadding;
    }

    Texture2D objTexture : register(t0);
    Texture2D shadowMap : register(t2);
    SamplerState objSampler : register(s0);
    SamplerComparisonState shadowSampler : register(s1);

    struct VSOutput {
        float4 position : SV_POSITION;
        float2 texCoord : TEXCOORD0;
        float3 worldNormal : TEXCOORD1;
        float3 worldPosition : TEXCOORD2;
        float4 shadowPos : TEXCOORD3;
    };

    float4 PSMain(VSOutput input) : SV_TARGET {
        float3 normal = normalize(input.worldNormal);
        float3 lightDir = normalize(-lightDirection);
        float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
        float3 reflectLightDir = reflect(-lightDir, normal);
        
        float3 ambient = lightAmbient.rgb * materialAmbient.rgb;
        float diff = max(dot(normal, lightDir), 0.0f);
        float3 diffuse = lightDiffuse.rgb * diff * materialDiffuse.rgb;
        float spec = pow(max(dot(viewDir, reflectLightDir), 0.0f), shininess);
        float3 specular = lightSpecular.rgb * spec * materialSpecular.rgb;
        
        float shadowFactor = 1.0f;
        if (useShadow != 0) {
            float3 projCoords = input.shadowPos.xyz / input.shadowPos.w;
            
            projCoords.x = projCoords.x * 0.5f + 0.5f;
            projCoords.y = projCoords.y * -0.5f + 0.5f;  // Инвертируем Y для DirectX
            
            // Применяем bias для избежания shadow acne
            float bias = shadowBias * tan(acos(saturate(diff)));
            bias = clamp(bias, 0.0f, 0.0005f);
            projCoords.z -= bias;
            
            if (projCoords.x >= 0.0f && projCoords.x <= 1.0f &&
                projCoords.y >= 0.0f && projCoords.y <= 1.0f) {
                
                // PCF фильтрация: 4 сэмпла для мягких теней
                float2 texelSize = float2(1.0f / 4096.0f, 1.0f / 4096.0f);
                
                shadowFactor = 0.0f;
                shadowFactor += shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy + float2(-0.5f, -0.5f) * texelSize, projCoords.z);
                shadowFactor += shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy + float2(0.5f, -0.5f) * texelSize, projCoords.z);
                shadowFactor += shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy + float2(-0.5f, 0.5f) * texelSize, projCoords.z);
                shadowFactor += shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy + float2(0.5f, 0.5f) * texelSize, projCoords.z);
                shadowFactor *= 0.25f;
                
                shadowFactor = saturate(shadowFactor);
            }
        }
        
        float3 result = ambient + (diffuse + specular) * shadowFactor;
        
        float4 texColor = objTexture.Sample(objSampler, input.texCoord);
        result *= texColor.rgb;
        
        return float4(result, 1.0f);
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

        desc.ByteWidth = sizeof(VSConstantBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &vsConstantBuffer);

        desc.ByteWidth = sizeof(ShadowConstantBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &shadowConstantBuffer);

        // PS буфер с выравниванием
        UINT psBufferSize = sizeof(PSConstantBuffer);
        UINT alignedSize = (psBufferSize + 15) & ~15;
        desc.ByteWidth = alignedSize;
        game->Device->CreateBuffer(&desc, nullptr, &psConstantBuffer);

        desc.ByteWidth = sizeof(MaterialBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &materialBuffer);

        desc.ByteWidth = sizeof(DirectionalLightBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &lightBuffer);

        // Сэмплер
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        game->Device->CreateSamplerState(&samplerDesc, &samplerState);
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

        // VS буфер
        VSConstantBuffer vsCB;
        Matrix world = Matrix::Identity;
        vsCB.world = world.Transpose();
        vsCB.view = game->Camera->GetViewMatrix().Transpose();
        vsCB.projection = game->Camera->GetProjectionMatrix().Transpose();
        Matrix worldInv = world;
        worldInv.Invert();
        vsCB.worldInvTranspose = worldInv.Transpose();
        game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsCB, 0, 0);

        // Shadow буфер
        ShadowConstantBuffer shadowCB;
        shadowCB.lightView = game->GetLightViewMatrix().Transpose();
        shadowCB.lightProjection = game->GetLightProjectionMatrix().Transpose();
        game->Context->UpdateSubresource(shadowConstantBuffer, 0, nullptr, &shadowCB, 0, 0);

        // PS буфер
        PSConstantBuffer psCB;
        Vector3 camPos = game->Camera->GetPosition();
        psCB.cameraPosition = DirectX::XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);
        psCB.objectColor = DirectX::XMFLOAT4(1, 1, 1, 1);
        psCB.useTexture = textureLoaded ? 1 : 0;
        psCB.hasMaterial = 0;
        psCB.useReflection = 0;
        psCB.useShadow = (game->ShadowMapSRV != nullptr) ? 1 : 0;
        psCB.shadowBias = 0.000005f;
        psCB.padding = 0.0f;
        game->Context->UpdateSubresource(psConstantBuffer, 0, nullptr, &psCB, 0, 0);

        // Material буфер
        MaterialBuffer matBuffer;
        matBuffer.ambient = Vector4(0.2f, 0.2f, 0.2f, 1.0f);
        matBuffer.diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        matBuffer.specular = Vector4(0.1f, 0.1f, 0.1f, 1.0f);
        matBuffer.shininess = 32.0f;
        matBuffer.materialPadding[0] = matBuffer.materialPadding[1] = matBuffer.materialPadding[2] = 0.0f;
        game->Context->UpdateSubresource(materialBuffer, 0, nullptr, &matBuffer, 0, 0);

        // Light буфер
        DirectionalLightBuffer lightBuf;
        lightBuf.ambient = game->SunLight.ambient;
        lightBuf.diffuse = game->SunLight.diffuse;
        lightBuf.specular = game->SunLight.specular;
        lightBuf.direction = game->SunLight.direction;
        lightBuf.padding = 0.0f;
        game->Context->UpdateSubresource(lightBuffer, 0, nullptr, &lightBuf, 0, 0);

        // Устанавливаем геометрию
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        game->Context->IASetInputLayout(inputLayout);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Устанавливаем шейдеры и буферы
        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);
        game->Context->VSSetConstantBuffers(1, 1, &shadowConstantBuffer);

        game->Context->PSSetShader(pixelShader, nullptr, 0);
        game->Context->PSSetConstantBuffers(0, 1, &psConstantBuffer);
        game->Context->PSSetConstantBuffers(1, 1, &materialBuffer);
        game->Context->PSSetConstantBuffers(2, 1, &lightBuffer);
        game->Context->PSSetSamplers(0, 1, &samplerState);

        // Устанавливаем текстуры
        if (textureLoaded && textureView) {
            game->Context->PSSetShaderResources(0, 1, &textureView);
        }

        // Устанавливаем shadow map + shadow sampler
        if (game->ShadowMapSRV) {
            game->Context->PSSetShaderResources(2, 1, &game->ShadowMapSRV);
            game->Context->PSSetSamplers(1, 1, &game->ShadowSampler);
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
        if (vsConstantBuffer) { vsConstantBuffer->Release(); vsConstantBuffer = nullptr; }
        if (shadowConstantBuffer) { shadowConstantBuffer->Release(); shadowConstantBuffer = nullptr; }
        if (psConstantBuffer) { psConstantBuffer->Release(); psConstantBuffer = nullptr; }
        if (materialBuffer) { materialBuffer->Release(); materialBuffer = nullptr; }
        if (lightBuffer) { lightBuffer->Release(); lightBuffer = nullptr; }
        if (samplerState) { samplerState->Release(); samplerState = nullptr; }
        if (textureView) { textureView->Release(); textureView = nullptr; }
        initialized = false;
        textureLoaded = false;
    }
};