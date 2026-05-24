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
    // Vertex Shader
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
            float4 color : COLOR;
        };

        struct VSOutput {
            float4 position : SV_POSITION;
            float2 texCoord : TEXCOORD0;
            float3 worldNormal : TEXCOORD1;
            float3 worldPosition : TEXCOORD2;
            float4 color : COLOR;
        };

        VSOutput VSMain(VSInput input) {
            VSOutput output;
            float4 worldPos = mul(float4(input.position, 1.0f), world);
            output.worldPosition = worldPos.xyz;
            output.position = mul(worldPos, view);
            output.position = mul(output.position, projection);
            output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldInvTranspose).xyz);
            output.texCoord = input.texCoord;
            output.color = input.color;
            return output;
        }
    )";

    // Pixel Shader
    const char* psCode = R"(
        struct VSOutput {
            float4 position : SV_POSITION;
            float2 texCoord : TEXCOORD0;
            float3 worldNormal : TEXCOORD1;
            float3 worldPosition : TEXCOORD2;
            float4 color : COLOR;
        };

        struct GBufferOutput {
            float4 diffuse  : SV_TARGET0;
            float4 normal   : SV_TARGET1;
            float4 worldPos : SV_TARGET2;
            float4 specular : SV_TARGET3;
        };

        Texture2D objTexture : register(t0);
        SamplerState objSampler : register(s0);

        GBufferOutput PSMain(VSOutput input) {
            GBufferOutput output;
            
            float4 texColor = objTexture.Sample(objSampler, input.texCoord);
            
            // Если текстура есть - используем её, иначе используем вершинный цвет
            if (texColor.r + texColor.g + texColor.b < 0.01f) {
                texColor = input.color;
            }
            
            output.diffuse = texColor;
            output.normal = float4(normalize(input.worldNormal), 1.0f);
            output.worldPos = float4(input.worldPosition, 1.0f);
            output.specular = float4(0.5f, 0.5f, 0.5f, 32.0f / 255.0f);
            
            return output;
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
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    game->Device->CreateInputLayout(elements, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    vsBlob->Release();
    psBlob->Release();
}

void TexturedGround::CreateBuffers() {
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    desc.ByteWidth = sizeof(VSConstantBuffer);
    game->Device->CreateBuffer(&desc, nullptr, &vsConstantBuffer);

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    game->Device->CreateSamplerState(&samplerDesc, &samplerState);
}

void TexturedGround::Initialize() {
    if (initialized) return;

    CreateGeometry();
    CreateShaders();
    CreateBuffers();

    textureView = Core::TextureLoader::LoadTexture2D(game, texturePath, false);
    textureLoaded = (textureView != nullptr);

    if (textureLoaded) {
        std::cout << "Ground texture loaded: " << texturePath << std::endl;
    }
    else {
        std::cout << "ERROR: Ground texture NOT loaded: " << texturePath << std::endl;
    }

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

    std::cout << "TexturedGround::DrawGeometry called" << std::endl;

    // Сохраняем текущие шейдеры и ресурсы
    ID3D11PixelShader* oldPS = nullptr;
    ID3D11VertexShader* oldVS = nullptr;
    ID3D11SamplerState* oldSampler = nullptr;
    ID3D11ShaderResourceView* oldSRV = nullptr;

    game->Context->PSGetShader(&oldPS, nullptr, nullptr);
    game->Context->VSGetShader(&oldVS, nullptr, nullptr);
    game->Context->PSGetSamplers(0, 1, &oldSampler);
    game->Context->PSGetShaderResources(0, 1, &oldSRV);

    // Устанавливаем текстуру и сэмплер
    if (textureLoaded && textureView) {
        game->Context->PSSetShaderResources(0, 1, &textureView);
        game->Context->PSSetSamplers(0, 1, &samplerState);
        std::cout << "Ground texture set in DrawGeometry, textureView=" << textureView << std::endl;
    }
    else {
        std::cout << "Ground texture NOT set in DrawGeometry" << std::endl;
    }

    Matrix world = Matrix::Identity;

    // Рисуем через RenderingSystem
    rs->DrawMeshToGBuffer(game->Context,
        vertexBuffer,
        indexBuffer,
        indexCount,
        world);

    // Восстанавливаем
    game->Context->PSSetShader(oldPS, nullptr, 0);
    game->Context->VSSetShader(oldVS, nullptr, 0);
    if (oldSampler) game->Context->PSSetSamplers(0, 1, &oldSampler);
    if (oldSRV) game->Context->PSSetShaderResources(0, 1, &oldSRV);

    if (oldPS) oldPS->Release();
    if (oldVS) oldVS->Release();
    if (oldSampler) oldSampler->Release();
    if (oldSRV) oldSRV->Release();

    std::cout << "TexturedGround::DrawGeometry completed" << std::endl;
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