#pragma once
#include "Game.h"
#include "Core.h"
#include "Lighting.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

namespace Render {

    class ShaderRenderer {
    protected:
        ID3D11InputLayout* inputLayout = nullptr;
        ID3D11VertexShader* vertexShader = nullptr;
        ID3D11PixelShader* pixelShader = nullptr;
        ID3D11Buffer* vsConstantBuffer = nullptr;
        ID3D11Buffer* psConstantBuffer = nullptr;
        ID3D11Buffer* materialBuffer = nullptr;
        ID3D11Buffer* lightBuffer = nullptr;
        ID3D11SamplerState* samplerState = nullptr;
        bool shadersInitialized = false;

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

        void CreateStandardBuffers(Game* game) {
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

        void UpdateVSConstantBuffer(Game* game, const Matrix& world, const Matrix& view, const Matrix& projection) {
            VSConstantBuffer vsCB;
            vsCB.world = world.Transpose();
            vsCB.view = view.Transpose();
            vsCB.projection = projection.Transpose();

            Matrix worldInv = world;
            worldInv.Invert();
            vsCB.worldInvTranspose = worldInv.Transpose();

            game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsCB, 0, 0);
        }

        void UpdatePSConstantBuffer(Game* game, const Vector4& color, bool useTexture, bool hasMaterial) {
            PSConstantBuffer psCB;
            Vector3 camPos = game->Camera->GetPosition();
            psCB.cameraPosition = Vector4(camPos.x, camPos.y, camPos.z, 1.0f);
            psCB.objectColor = color;
            psCB.useTexture = useTexture ? 1 : 0;
            psCB.hasMaterial = hasMaterial ? 1 : 0;
            psCB.useReflection = 0;
            psCB.padding = 0.0f;
            game->Context->UpdateSubresource(psConstantBuffer, 0, nullptr, &psCB, 0, 0);
        }

        void UpdateMaterialBuffer(Game* game, const Material& mat) {
            MaterialBuffer matBuffer;
            matBuffer.ambient = mat.ambient;
            matBuffer.diffuse = mat.diffuse;
            matBuffer.specular = mat.specular;
            matBuffer.shininess = mat.shininess;
            matBuffer.padding[0] = matBuffer.padding[1] = matBuffer.padding[2] = 0.0f;
            game->Context->UpdateSubresource(materialBuffer, 0, nullptr, &matBuffer, 0, 0);
        }

        void UpdateLightBuffer(Game* game, const DirectionalLight& light) {
            DirectionalLightBuffer lightBufferData;
            lightBufferData.ambient = light.ambient;
            lightBufferData.diffuse = light.diffuse;
            lightBufferData.specular = light.specular;
            lightBufferData.direction = light.direction;
            lightBufferData.padding = 0.0f;
            game->Context->UpdateSubresource(lightBuffer, 0, nullptr, &lightBufferData, 0, 0);
        }

        void BindStandardResources(Game* game) {
            game->Context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);
            game->Context->PSSetConstantBuffers(0, 1, &psConstantBuffer);
            game->Context->PSSetConstantBuffers(1, 1, &materialBuffer);
            game->Context->PSSetConstantBuffers(2, 1, &lightBuffer);
            game->Context->PSSetSamplers(0, 1, &samplerState);
        }

    public:
        virtual ~ShaderRenderer() {
            DestroyResources();
        }

        virtual void DestroyResources() {
            if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
            if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
            if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
            if (vsConstantBuffer) { vsConstantBuffer->Release(); vsConstantBuffer = nullptr; }
            if (psConstantBuffer) { psConstantBuffer->Release(); psConstantBuffer = nullptr; }
            if (materialBuffer) { materialBuffer->Release(); materialBuffer = nullptr; }
            if (lightBuffer) { lightBuffer->Release(); lightBuffer = nullptr; }
            if (samplerState) { samplerState->Release(); samplerState = nullptr; }
            shadersInitialized = false;
        }
    };

} // namespace Render