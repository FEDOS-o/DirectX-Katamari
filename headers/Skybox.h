#pragma once
#include "GameComponent.h"
#include "Camera.h"
#include "TextureLoader.h"
#include <SimpleMath.h>
#include <string>

using namespace DirectX::SimpleMath;

class Skybox : public GameComponent {
private:
    struct Vertex {
        Vector3 position;
    };

    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11Buffer* constantBuffer = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    ID3D11ShaderResourceView* cubeTextureView = nullptr;
    ID3D11RasterizerState* rasterizerState = nullptr;
    ID3D11DepthStencilState* depthStencilState = nullptr;

    UINT indexCount = 0;
    bool initialized = false;
    std::string texturePath;

#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter
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
#pragma warning(pop)

    void CreateCube() {
        const float SIZE = 100000.0f;
        const Vertex VERTICES[] = {
            { Vector3(-SIZE, -SIZE,  SIZE) }, { Vector3(SIZE, -SIZE,  SIZE) },
            { Vector3(SIZE,  SIZE,  SIZE) }, { Vector3(-SIZE,  SIZE,  SIZE) },
            { Vector3(-SIZE, -SIZE, -SIZE) }, { Vector3(SIZE, -SIZE, -SIZE) },
            { Vector3(SIZE,  SIZE, -SIZE) }, { Vector3(-SIZE,  SIZE, -SIZE) },
        };

        const UINT INDICES[] = {
            0, 1, 2, 0, 2, 3, 1, 5, 6, 1, 6, 2,
            5, 4, 7, 5, 7, 6, 4, 0, 3, 4, 3, 7,
            3, 2, 6, 3, 6, 7, 4, 5, 1, 4, 1, 0,
        };

        indexCount = sizeof(INDICES) / sizeof(UINT);

        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;

        desc.ByteWidth = sizeof(VERTICES);
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA data = { VERTICES };
        game->Device->CreateBuffer(&desc, &data, &vertexBuffer);

        desc.ByteWidth = sizeof(INDICES);
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        data.pSysMem = INDICES;
        game->Device->CreateBuffer(&desc, &data, &indexBuffer);
    }

    void CreateShaders() {
        const char* vsCode = R"(
            cbuffer ConstantBuffer : register(b0) { float4x4 worldViewProj; };
            struct VSInput { float3 position : POSITION; };
            struct VSOutput {
                float4 position : SV_POSITION;
                float3 texCoord : TEXCOORD0;
            };
            VSOutput VSMain(VSInput input) {
                VSOutput output;
                output.position = mul(float4(input.position, 1.0f), worldViewProj);
                output.position = output.position.xyww;
                output.texCoord = input.position;
                return output;
            }
        )";

        const char* psCode = R"(
            TextureCube cubeTexture : register(t0);
            SamplerState cubeSampler : register(s0);
            struct VSOutput {
                float4 position : SV_POSITION;
                float3 texCoord : TEXCOORD0;
            };
            float4 PSMain(VSOutput input) : SV_TARGET {
                return cubeTexture.Sample(cubeSampler, input.texCoord);
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
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        game->Device->CreateInputLayout(elements, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

        vsBlob->Release();
        psBlob->Release();

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.ByteWidth = sizeof(Matrix);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        game->Device->CreateBuffer(&cbDesc, nullptr, &constantBuffer);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        game->Device->CreateSamplerState(&samplerDesc, &samplerState);

        D3D11_RASTERIZER_DESC rastDesc = {};
        rastDesc.CullMode = D3D11_CULL_FRONT;
        rastDesc.FillMode = D3D11_FILL_SOLID;
        rastDesc.DepthClipEnable = true;
        rastDesc.FrontCounterClockwise = true;
        game->Device->CreateRasterizerState(&rastDesc, &rasterizerState);

        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        game->Device->CreateDepthStencilState(&dsDesc, &depthStencilState);
    }

public:
    Skybox(Game* game, const std::string& textureFile = "models/cubemap.png")
        : GameComponent(game), texturePath(textureFile) {
    }

    void Initialize() override {
        if (initialized) return;

        CreateCube();
        CreateShaders();

        int faceSize;
        cubeTextureView = Core::TextureLoader::LoadCubeTexture(game, texturePath, faceSize);
        if (cubeTextureView) {
            game->SkyboxTexture = cubeTextureView;
        }

        initialized = true;
    }

    void Update(float deltaTime) override {}

    void Draw() override {
        if (!initialized || !game || !game->Context || !game->Camera) return;
        if (!cubeTextureView) return;

        ID3D11DeviceContext* context = game->Context;

        ID3D11DepthStencilState* oldDepthState = nullptr;
        UINT oldStencilRef = 0;
        context->OMGetDepthStencilState(&oldDepthState, &oldStencilRef);

        ID3D11RasterizerState* oldRasterState = nullptr;
        context->RSGetState(&oldRasterState);

        context->RSSetState(rasterizerState);
        context->OMSetDepthStencilState(depthStencilState, 0);

        Matrix view = game->Camera->GetViewMatrix();
        Matrix projection = game->Camera->GetProjectionMatrix();
        Matrix wvp = Matrix::Identity * view * projection;
        Matrix transposed = wvp.Transpose();
        context->UpdateSubresource(constantBuffer, 0, nullptr, &transposed, 0, 0);

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        context->IASetInputLayout(inputLayout);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context->VSSetShader(vertexShader, nullptr, 0);
        context->VSSetConstantBuffers(0, 1, &constantBuffer);
        context->PSSetShader(pixelShader, nullptr, 0);
        context->PSSetShaderResources(0, 1, &cubeTextureView);
        context->PSSetSamplers(0, 1, &samplerState);

        context->DrawIndexed(indexCount, 0, 0);

        context->RSSetState(oldRasterState);
        context->OMSetDepthStencilState(oldDepthState, oldStencilRef);

        if (oldRasterState) oldRasterState->Release();
        if (oldDepthState) oldDepthState->Release();
    }

    void DestroyResources() override {
        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
        if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
        if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
        if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
        if (constantBuffer) { constantBuffer->Release(); constantBuffer = nullptr; }
        if (samplerState) { samplerState->Release(); samplerState = nullptr; }
        if (cubeTextureView) { cubeTextureView->Release(); cubeTextureView = nullptr; }
        if (rasterizerState) { rasterizerState->Release(); rasterizerState = nullptr; }
        if (depthStencilState) { depthStencilState->Release(); depthStencilState = nullptr; }

        if (game && game->SkyboxTexture == cubeTextureView) {
            game->SkyboxTexture = nullptr;
        }

        initialized = false;
    }
};