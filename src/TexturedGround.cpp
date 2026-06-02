// TexturedGround.cpp
#include "TexturedGround.h"
#include "Game.h"

TexturedGround::TexturedGround(Game* game, float inSize, int inSegments, const std::string& textureFile)
    : GameComponent(game)
    , size(inSize)
    , segments(inSegments)
    , texturePath(textureFile)
    , vertexBuffer(nullptr)
    , indexBuffer(nullptr)
    , inputLayout(nullptr)
    , vertexShader(nullptr)
    , pixelShader(nullptr)
    , vsConstantBuffer(nullptr)
    , psConstantBuffer(nullptr)
    , materialBuffer(nullptr)
    , lightBuffer(nullptr)
    , samplerState(nullptr)
    , textureView(nullptr)
    , indexCount(0)
    , initialized(false)
    , textureLoaded(false) {
}

TexturedGround::~TexturedGround() {
    DestroyResources();
}

ID3DBlob* TexturedGround::CompileShader(const char* code, const char* target, const char* entryPoint) {
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

void TexturedGround::CreateGeometry() {
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
            vert.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
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

void TexturedGround::CreateShaders() {
    // СОЗДАЁМ ТОЛЬКО VERTEX SHADER для geometry pass (совместимый с тем, что использует RenderingSystem)
    // Pixel шейдер будет использоваться из RenderingSystem

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

    ID3DBlob* vsBlob = CompileShader(vsCode, "vs_5_0", "VSMain");
    if (!vsBlob) return;

    game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    vsBlob->Release();

    // Создаём input layout
    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    game->Device->CreateInputLayout(elements, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    // Pixel шейдер НЕ СОЗДАЁМ - будем использовать тот, что в RenderingSystem
    pixelShader = nullptr;
}

void TexturedGround::CreateBuffers() {
    // Создаём сэмплер для текстуры
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    game->Device->CreateSamplerState(&samplerDesc, &samplerState);

    // Константный буфер для VS
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.ByteWidth = sizeof(VSConstantBuffer);
    game->Device->CreateBuffer(&desc, nullptr, &vsConstantBuffer);
}

void TexturedGround::Initialize() {
    if (initialized) return;

    CreateGeometry();
    CreateShaders();
    CreateBuffers();

    textureView = Core::TextureLoader::LoadTexture2D(game, texturePath, false);
    textureLoaded = (textureView != nullptr);


    initialized = true;
}

void TexturedGround::Update(float deltaTime) {
    (void)deltaTime;
}

void TexturedGround::Draw() {
    // В Deferred режиме Draw() не используется
}

void TexturedGround::DrawGeometry(RenderingSystem* rs) {
    if (!initialized || !rs || !vertexBuffer || !indexBuffer) return;


    // НЕ устанавливаем свой vertex shader!
    // Используем тот, что уже установлен в BeginGeometryPass

    // Просто устанавливаем текстуру и сэмплер
    if (textureLoaded && textureView) {
        game->Context->PSSetShaderResources(0, 1, &textureView);
        game->Context->PSSetSamplers(0, 1, &samplerState);
    }

    // Обновляем константный буфер с мировыми матрицами
    VSConstantBuffer cb;
    Matrix world = Matrix::Identity;
    cb.world = world.Transpose();
    cb.view = rs->GetViewMatrix().Transpose();
    cb.projection = rs->GetProjectionMatrix().Transpose();

    Matrix worldInv = world;
    worldInv.Invert();
    cb.worldInvTranspose = worldInv.Transpose();

    // Обновляем буфер, который использует RenderingSystem
    ID3D11Buffer* vsBuffer = nullptr;
    game->Context->VSGetConstantBuffers(0, 1, &vsBuffer);
    if (vsBuffer) {
        game->Context->UpdateSubresource(vsBuffer, 0, nullptr, &cb, 0, 0);
        vsBuffer->Release();
    }

    // Рисуем
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    game->Context->DrawIndexed(indexCount, 0, 0);

    // Очищаем текстуру
    ID3D11ShaderResourceView* nullSRV = nullptr;
    game->Context->PSSetShaderResources(0, 1, &nullSRV);
}

void TexturedGround::DrawShadow() {
    // Ground не отбрасывает тени
}

void TexturedGround::DestroyResources() {
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