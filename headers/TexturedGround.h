#pragma once
#include "GameComponent.h"
#include "TextureLoader.h"
#include <SimpleMath.h>
#include <vector>
#include <d3dcompiler.h>

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
    ID3D11Buffer* vsConstantBuffer = nullptr;
    ID3D11Buffer* psConstantBuffer = nullptr;
    ID3D11Buffer* materialBuffer = nullptr;
    ID3D11Buffer* lightBuffer = nullptr;
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
        game->Device->CreateBuffer(&vertexDesc, &vertexData, &vertexBuffer);

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(UINT) * indexCount;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA indexData = { indices.data() };
        game->Device->CreateBuffer(&indexDesc, &indexData, &indexBuffer);
    }

    void CreateShaders() {
        const char* vsCode = R"(
            cbuffer VSConstantBuffer : register(b0) {
                float4x4 world;
                float4x4 view;
                float4x4 projection;
                float4x4 worldInvTranspose;
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
            };

            VSOutput VSMain(VSInput input) {
                VSOutput output;
                
                float4 worldPos = mul(float4(input.position, 1.0f), world);
                output.worldPosition = worldPos.xyz;
                output.position = mul(worldPos, view);
                output.position = mul(output.position, projection);
                
                output.worldNormal = mul(float4(input.normal, 0.0f), worldInvTranspose).xyz;
                output.worldNormal = normalize(output.worldNormal);
                
                output.texCoord = input.texCoord;
                
                return output;
            }
        )";

        const char* psCode = R"(
            cbuffer PSConstantBuffer : register(b0) {
                float4 cameraPosition;
                float4 objectColor;
                int useTexture;
                int hasMaterial;
                float2 padding;
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
            SamplerState objSampler : register(s0);
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float2 texCoord : TEXCOORD0;
                float3 worldNormal : TEXCOORD1;
                float3 worldPosition : TEXCOORD2;
            };
            
            float4 PSMain(VSOutput input) : SV_TARGET {
                float3 normal = normalize(input.worldNormal);
                float3 lightDir = normalize(-lightDirection);
                float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
                float3 reflectDir = reflect(-lightDir, normal);
                
                float3 ambient = lightAmbient.rgb * materialAmbient.rgb;
                float diff = max(dot(normal, lightDir), 0.0f);
                float3 diffuse = lightDiffuse.rgb * diff * materialDiffuse.rgb;
                float spec = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);
                float3 specular = lightSpecular.rgb * spec * materialSpecular.rgb;
                float3 result = ambient + diffuse + specular;
                
                float4 texColor = float4(1, 1, 1, 1);
                if (useTexture != 0) {
                    texColor = objTexture.Sample(objSampler, input.texCoord);
                    result *= texColor.rgb;
                } else {
                    result *= materialDiffuse.rgb;
                }
                
                return float4(result, 1.0f);
            }
        )";

        ID3DBlob* vsBlob = CompileShader(vsCode, "vs_5_0", "VSMain");
        ID3DBlob* psBlob = CompileShader(psCode, "ps_5_0", "PSMain");

        if (!vsBlob || !psBlob) {
            if (vsBlob) vsBlob->Release();
            if (psBlob) psBlob->Release();
            return;
        }

        game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

        D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
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

        desc.ByteWidth = sizeof(PSConstantBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &psConstantBuffer);

        desc.ByteWidth = sizeof(MaterialBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &materialBuffer);

        desc.ByteWidth = sizeof(DirectionalLightBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &lightBuffer);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        game->Device->CreateSamplerState(&samplerDesc, &samplerState);
    }

    void UpdateLight(const DirectionalLight& light) {
        if (!lightBuffer || !game || !game->Context) return;

        DirectionalLightBuffer lightBufferData;
        lightBufferData.ambient = light.ambient;
        lightBufferData.diffuse = light.diffuse;
        lightBufferData.specular = light.specular;
        lightBufferData.direction = light.direction;
        lightBufferData.padding = 0.0f;

        game->Context->UpdateSubresource(lightBuffer, 0, nullptr, &lightBufferData, 0, 0);
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

    void Update(float deltaTime) override {
        UpdateLight(game->SunLight);
    }

    void Draw() override {
        if (!initialized || !game || !game->Context || !game->Camera) return;
        if (!vertexBuffer || !indexBuffer) return;

        VSConstantBuffer vsCB;
        Matrix world = Matrix::Identity;
        vsCB.world = world.Transpose();
        vsCB.view = game->Camera->GetViewMatrix().Transpose();
        vsCB.projection = game->Camera->GetProjectionMatrix().Transpose();

        Matrix worldInv = world;
        worldInv.Invert();
        vsCB.worldInvTranspose = worldInv.Transpose();

        game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsCB, 0, 0);

        PSConstantBuffer psCB;
        Vector3 camPos = game->Camera->GetPosition();
        psCB.cameraPosition = Vector4(camPos.x, camPos.y, camPos.z, 1.0f);
        psCB.objectColor = Vector4(1, 1, 1, 1);
        psCB.useTexture = textureLoaded ? 1 : 0;
        psCB.hasMaterial = 1;
        psCB.padding = 0.0f;

        game->Context->UpdateSubresource(psConstantBuffer, 0, nullptr, &psCB, 0, 0);

        MaterialBuffer matBuffer;
        matBuffer.ambient = Vector4(0.6f, 0.6f, 0.6f, 1.0f);
        matBuffer.diffuse = Vector4(1.0f, 0.9f, 0.7f, 1.0f);
        matBuffer.specular = Vector4(0.2f, 0.2f, 0.2f, 1.0f);
        matBuffer.shininess = 32.0f;
        matBuffer.padding[0] = matBuffer.padding[1] = matBuffer.padding[2] = 0.0f;

        game->Context->UpdateSubresource(materialBuffer, 0, nullptr, &matBuffer, 0, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        game->Context->IASetInputLayout(inputLayout);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);

        game->Context->PSSetShader(pixelShader, nullptr, 0);
        game->Context->PSSetConstantBuffers(0, 1, &psConstantBuffer);
        game->Context->PSSetConstantBuffers(1, 1, &materialBuffer);
        game->Context->PSSetConstantBuffers(2, 1, &lightBuffer);
        game->Context->PSSetSamplers(0, 1, &samplerState);

        if (textureLoaded && textureView) {
            game->Context->PSSetShaderResources(0, 1, &textureView);
        }

        game->Context->DrawIndexed(indexCount, 0, 0);
    }

    void DestroyResources() override {
        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
        if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
        if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
        if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
        if (vsConstantBuffer) { vsConstantBuffer->Release(); vsConstantBuffer = nullptr; }
        if (psConstantBuffer) { psConstantBuffer->Release(); psConstantBuffer = nullptr; }
        if (materialBuffer) { materialBuffer->Release(); materialBuffer = nullptr; }
        if (lightBuffer) { lightBuffer->Release(); lightBuffer = nullptr; }
        if (samplerState) { samplerState->Release(); samplerState = nullptr; }
        if (textureView) { textureView->Release(); textureView = nullptr; }
        initialized = false;
        textureLoaded = false;
    }
};