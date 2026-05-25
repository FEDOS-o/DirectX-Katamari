// Skybox.h
#pragma once
#include "GameComponent.h"
#include "Camera.h"
#include "TextureLoader.h"
#include <SimpleMath.h>
#include <string>
#include <iostream>

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
        if (error) error->Release();
        return blob;
    }

    void CreateCube() {
        const float SIZE = 1000.0f;
        const Vertex VERTICES[] = {
            { Vector3(-SIZE, -SIZE,  SIZE) }, { Vector3(SIZE, -SIZE,  SIZE) },
            { Vector3(SIZE,  SIZE,  SIZE) }, { Vector3(-SIZE,  SIZE,  SIZE) },
            { Vector3(-SIZE, -SIZE, -SIZE) }, { Vector3(SIZE, -SIZE, -SIZE) },
            { Vector3(SIZE,  SIZE, -SIZE) }, { Vector3(-SIZE,  SIZE, -SIZE) },
        };

        const UINT INDICES[] = {
            0, 1, 2, 0, 2, 3,  // front
            1, 5, 6, 1, 6, 2,  // right
            5, 4, 7, 5, 7, 6,  // back
            4, 0, 3, 4, 3, 7,  // left
            3, 2, 6, 3, 6, 7,  // top
            4, 5, 1, 4, 1, 0,  // bottom
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
            cbuffer ConstantBuffer : register(b0) {
                float4x4 viewProjection;
            }
            
            struct VSInput {
                float3 position : POSITION;
            };
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float3 texCoord : TEXCOORD0;
            };
            
            VSOutput VSMain(VSInput input) {
                VSOutput output;
                
                float4x4 viewProjNoTranslate = viewProjection;
                viewProjNoTranslate[3][0] = 0;
                viewProjNoTranslate[3][1] = 0;
                viewProjNoTranslate[3][2] = 0;
                
                output.position = mul(float4(input.position, 1.0f), viewProjNoTranslate);
                // КЛЮЧЕВОЙ МОМЕНТ: устанавливаем глубину в максимальное значение (1.0)
                output.position.z = output.position.w;
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
        rastDesc.CullMode = D3D11_CULL_BACK;
        rastDesc.FillMode = D3D11_FILL_SOLID;
        rastDesc.DepthClipEnable = true;
        rastDesc.FrontCounterClockwise = true;
        game->Device->CreateRasterizerState(&rastDesc, &rasterizerState);

        // КЛЮЧЕВОЙ МОМЕНТ: Depth state для skybox
        // DepthWrite = OFF (не пишем в depth buffer)
        // DepthFunc = GREATER_EQUAL (рисуем только если глубина сцены >= глубины skybox)
        // Но так как глубина skybox = 1.0 (максимум), то он будет рисоваться только там, где глубина >= 1.0,
        // то есть там, где ничего нет. Но это не работает, потому что глубина сцены < 1.0.
        // Правильно: DepthFunc = ALWAYS, но с DepthWrite = OFF, и рисовать skybox ПЕРВЫМ.
        // Или использовать LESS_EQUAL с глубиной 1.0.

        // Вариант 1: Рисуем skybox ПЕРВЫМ, DepthWrite = OFF, DepthFunc = LESS_EQUAL
        // Тогда skybox запишется в color buffer, но не в depth buffer,
        // и объекты поверх него перерисуются.
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = true;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // НЕ пишем в depth buffer
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;       // Рисуем если глубина <= существующей
        dsDesc.StencilEnable = false;
        game->Device->CreateDepthStencilState(&dsDesc, &depthStencilState);
    }

public:
    Skybox(Game* game, const std::string& textureFile = "models/cubemap.png")
        : GameComponent(game), texturePath(textureFile), initialized(false) {
        vertexBuffer = nullptr;
        indexBuffer = nullptr;
        inputLayout = nullptr;
        vertexShader = nullptr;
        pixelShader = nullptr;
        constantBuffer = nullptr;
        samplerState = nullptr;
        cubeTextureView = nullptr;
        rasterizerState = nullptr;
        depthStencilState = nullptr;
        indexCount = 0;
    }

    ~Skybox() {
        DestroyResources();
    }

    void Initialize() override {
        if (initialized) return;
        if (!game || !game->Device) return;

        CreateCube();
        CreateShaders();

        int faceSize;
        cubeTextureView = Core::TextureLoader::LoadCubeTexture(game, texturePath, faceSize);
        if (cubeTextureView) {
            game->SkyboxTexture = cubeTextureView;
            std::cout << "Skybox texture loaded: " << texturePath << std::endl;
        }
        else {
            std::cout << "ERROR: Skybox texture NOT loaded: " << texturePath << std::endl;
        }

        initialized = true;
    }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    void Draw() override {
        if (!initialized || !game || !game->Context || !game->Camera) return;
        if (!cubeTextureView) return;

        ID3D11DeviceContext* context = game->Context;

        // Сохраняем старые состояния
        ID3D11DepthStencilState* oldDepthState = nullptr;
        UINT oldStencilRef = 0;
        context->OMGetDepthStencilState(&oldDepthState, &oldStencilRef);

        ID3D11RasterizerState* oldRasterState = nullptr;
        context->RSGetState(&oldRasterState);

        // Устанавливаем skybox состояния
        context->RSSetState(rasterizerState);
        context->OMSetDepthStencilState(depthStencilState, 0);

        Matrix view = game->Camera->GetViewMatrix();
        Matrix projection = game->Camera->GetProjectionMatrix();

        // Убираем трансляцию
        view._41 = 0;
        view._42 = 0;
        view._43 = 0;

        Matrix viewProj = view * projection;
        Matrix transposed = viewProj.Transpose();
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

        // Восстанавливаем старые состояния
        context->RSSetState(oldRasterState);
        context->OMSetDepthStencilState(oldDepthState, oldStencilRef);

        if (oldRasterState) oldRasterState->Release();
        if (oldDepthState) oldDepthState->Release();
    }

    void DrawGeometry(RenderingSystem* rs) override {
        // Не участвует в GBuffer
        (void)rs;
    }

    void DrawShadow() override {
        // Не отбрасывает тени
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