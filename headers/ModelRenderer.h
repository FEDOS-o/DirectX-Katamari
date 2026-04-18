#pragma once
#include "MaterialManager.h"
#include "MeshManager.h"
#include "Lighting.h"
#include "Camera.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

class ModelRenderer {
private:
    Game* game;

    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11Buffer* vsConstantBuffer = nullptr;
    ID3D11Buffer* psConstantBuffer = nullptr;
    ID3D11Buffer* materialBuffer = nullptr;
    ID3D11Buffer* lightBuffer = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    bool initialized = false;

    ID3DBlob* CompileShader(const char* code, const char* target, const char* entryPoint) {
        ID3DBlob* blob = nullptr;
        ID3DBlob* error = nullptr;
        HRESULT hr = D3DCompile(code, strlen(code), nullptr, nullptr, nullptr,
            entryPoint, target, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &blob, &error);
        if (FAILED(hr) && error) error->Release();
        return blob;
    }

    void InitializeShaders() {
        if (initialized || !game) return;

        const char* vsCode = R"(
            cbuffer VSConstantBuffer : register(b0) {
                float4x4 world;
                float4x4 view;
                float4x4 projection;
                float4x4 worldInvTranspose;
            }
            struct VSInput {
                float3 position : POSITION;
                float4 color : COLOR;
                float2 texCoord : TEXCOORD;
                float3 normal : NORMAL;
            };
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
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
                output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldInvTranspose).xyz);
                output.color = input.color;
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
                int useReflection;
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
            SamplerState objSampler : register(s0);
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
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
                } else if (hasMaterial != 0) {
                    result *= materialDiffuse.rgb;
                } else {
                    result *= input.color.rgb;
                }
                return float4(result, 1.0f);
            }
        )";

        ID3DBlob* vsBlob = CompileShader(vsCode, "vs_5_0", "VSMain");
        ID3DBlob* psBlob = CompileShader(psCode, "ps_5_0", "PSMain");
        if (!vsBlob || !psBlob) return;

        game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

        D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        game->Device->CreateInputLayout(elements, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

        vsBlob->Release();
        psBlob->Release();

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        bufferDesc.ByteWidth = sizeof(VSConstantBuffer);
        game->Device->CreateBuffer(&bufferDesc, nullptr, &vsConstantBuffer);
        bufferDesc.ByteWidth = sizeof(PSConstantBuffer);
        game->Device->CreateBuffer(&bufferDesc, nullptr, &psConstantBuffer);
        bufferDesc.ByteWidth = sizeof(MaterialBuffer);
        game->Device->CreateBuffer(&bufferDesc, nullptr, &materialBuffer);
        bufferDesc.ByteWidth = sizeof(DirectionalLightBuffer);
        game->Device->CreateBuffer(&bufferDesc, nullptr, &lightBuffer);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        game->Device->CreateSamplerState(&samplerDesc, &samplerState);

        initialized = true;
    }

    void UpdateLightBuffer(const DirectionalLight& light) {
        DirectionalLightBuffer lightData;
        lightData.ambient = light.ambient;
        lightData.diffuse = light.diffuse;
        lightData.specular = light.specular;
        lightData.direction = light.direction;
        lightData.padding = 0.0f;
        game->Context->UpdateSubresource(lightBuffer, 0, nullptr, &lightData, 0, 0);
    }

public:
    ModelRenderer(Game* inGame) : game(inGame) {}

    ~ModelRenderer() {
        if (vertexShader) vertexShader->Release();
        if (pixelShader) pixelShader->Release();
        if (inputLayout) inputLayout->Release();
        if (vsConstantBuffer) vsConstantBuffer->Release();
        if (psConstantBuffer) psConstantBuffer->Release();
        if (materialBuffer) materialBuffer->Release();
        if (lightBuffer) lightBuffer->Release();
        if (samplerState) samplerState->Release();
    }

    void Initialize() { InitializeShaders(); }

    void Draw(const std::vector<MeshData*>& meshes,
        const MaterialManager& materialManager,
        const Matrix& worldMatrix,
        const Matrix& view,
        const Matrix& projection,
        const DirectionalLight& light,
        const Vector3& cameraPos) {

        if (!initialized || meshes.empty()) return;

        UpdateLightBuffer(light);

        VSConstantBuffer vsCB;
        vsCB.world = worldMatrix.Transpose();
        vsCB.view = view.Transpose();
        vsCB.projection = projection.Transpose();
        Matrix worldInv = worldMatrix;
        worldInv.Invert();
        vsCB.worldInvTranspose = worldInv.Transpose();
        game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsCB, 0, 0);

        game->Context->IASetInputLayout(inputLayout);
        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);
        game->Context->PSSetShader(pixelShader, nullptr, 0);
        game->Context->PSSetConstantBuffers(1, 1, &materialBuffer);
        game->Context->PSSetConstantBuffers(2, 1, &lightBuffer);
        game->Context->PSSetSamplers(0, 1, &samplerState);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (MeshData* mesh : meshes) {
            if (!mesh->vertexBuffer || !mesh->indexBuffer) continue;

            const MaterialData& mat = materialManager.GetMaterial(mesh->materialIndex);
            MaterialBuffer matBuffer;
            matBuffer.ambient = mat.ambientColor;
            matBuffer.diffuse = mat.diffuseColor;
            matBuffer.specular = mat.specularColor;
            matBuffer.shininess = mat.shininess;
            game->Context->UpdateSubresource(materialBuffer, 0, nullptr, &matBuffer, 0, 0);

            ID3D11ShaderResourceView* texture = mat.hasTexture ?
                materialManager.GetTexture(mat.diffuseTexturePath) : nullptr;

            PSConstantBuffer psCB;
            psCB.cameraPosition = Vector4(cameraPos.x, cameraPos.y, cameraPos.z, 1.0f);
            psCB.objectColor = Vector4(1, 1, 1, 1);
            psCB.useTexture = texture ? 1 : 0;
            psCB.hasMaterial = 1;
            psCB.useReflection = 0;
            psCB.padding = 0.0f;
            game->Context->UpdateSubresource(psConstantBuffer, 0, nullptr, &psCB, 0, 0);
            game->Context->PSSetConstantBuffers(0, 1, &psConstantBuffer);
            game->Context->PSSetShaderResources(0, 1, &texture);

            MeshManager::DrawMesh(game->Context, mesh);
        }
    }
};