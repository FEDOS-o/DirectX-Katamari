// Particle.cpp
#include "Particle.h"
#include "Game.h"
#include "Camera.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

ID3DBlob* ParticleEmitter::CompileShader(const char* code, const char* target, const char* entry) {
    ID3DBlob* blob = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hr = D3DCompile(code, strlen(code), nullptr, nullptr, nullptr,
        entry, target, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &blob, &error);

    if (FAILED(hr) && error) {
        OutputDebugStringA((char*)error->GetBufferPointer());
        error->Release();
        return nullptr;
    }
    if (error) error->Release();
    return blob;
}

ParticleEmitter::ParticleEmitter(Game* game, const Vector3& position)
    : GameComponent(game)
    , emitterPosition(position)
    , emitterDirection(0, 1, 0)
    , emissionRate(50)
    , particlesPerSecond(50)
    , timeSinceLastEmission(0)
    , maxParticles(1000)
    , particleSpeedMin(3.0f)
    , particleSpeedMax(8.0f)
    , particleLifeMin(1.0f)
    , particleLifeMax(2.5f)
    , particleSizeMin(0.1f)
    , particleSizeMax(0.3f)
    , startColor(1.0f, 0.5f, 0.2f, 1.0f)
    , endColor(0.2f, 0.2f, 1.0f, 0.0f)
    , gravity(0, -15.0f, 0)
    , groundY(0)
    , bounceDamping(0.5f)
    , useGravity(true)
    , vertexBuffer(nullptr)
    , indexBuffer(nullptr)
    , inputLayout(nullptr)
    , vertexShader(nullptr)
    , pixelShader(nullptr)
    , vsConstantBuffer(nullptr)
    , additiveBlendState(nullptr)
    , depthState(nullptr)
    , currentParticleCount(0)
    , initialized(false) {

    std::random_device rd;
    rng.seed(rd());
    dist = std::uniform_real_distribution<float>(0.0f, 1.0f);
}

ParticleEmitter::~ParticleEmitter() {
    DestroyResources();
}

void ParticleEmitter::CreateGeometryBuffers() {
    if (!game || !game->Device) return;

    UINT maxVertices = maxParticles * 4;
    UINT maxIndices = maxParticles * 6;

    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexDesc.ByteWidth = sizeof(ParticleVertex) * maxVertices;
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    game->Device->CreateBuffer(&vertexDesc, nullptr, &vertexBuffer);

    // Индексный буфер для всех квадов
    std::vector<UINT> indices;
    indices.reserve(maxIndices);
    for (int i = 0; i < maxParticles; i++) {
        UINT base = i * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_DEFAULT;
    indexDesc.ByteWidth = sizeof(UINT) * maxIndices;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = { indices.data() };
    game->Device->CreateBuffer(&indexDesc, &indexData, &indexBuffer);
}

void ParticleEmitter::CreateShaders() {
    // Vertex Shader - передаем позицию в world space, а также размер и цвет
    const char* vsCode = R"(
        cbuffer VSConstantBuffer : register(b0) {
            float4x4 view;
            float4x4 projection;
        }
        
        struct VSInput {
            float3 position : POSITION;
            float4 color : COLOR;
        };
        
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
            float depth : TEXCOORD0;
        };
        
        VSOutput VSMain(VSInput input) {
            VSOutput output;
            float4 worldPos = float4(input.position, 1.0f);
            float4 viewPos = mul(worldPos, view);
            output.position = mul(viewPos, projection);
            output.color = input.color;
            output.depth = viewPos.z;  // Сохраняем глубину для отладки
            return output;
        }
    )";

    // Pixel Shader - просто возвращает цвет
    const char* psCode = R"(
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
            float depth : TEXCOORD0;
        };
        
        float4 PSMain(VSOutput input) : SV_TARGET {
            return input.color;
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

    // Input layout - только позиция и цвет
    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    game->Device->CreateInputLayout(elements, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    vsBlob->Release();
    psBlob->Release();

    // Константный буфер
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(Matrix) * 2; // view + projection
    game->Device->CreateBuffer(&cbDesc, nullptr, &vsConstantBuffer);
}

void ParticleEmitter::CreateStates() {
    if (!game || !game->Device) return;

    // Аддитивное смешивание (эффект свечения)
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = true;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    game->Device->CreateBlendState(&blendDesc, &additiveBlendState);

    // Depth state - частицы проверяют глубину, НО НЕ ПИШУТ в нее
    // Это позволяет частицам быть перекрытыми геометрией,
    // но не перекрывать друг друга (что для прозрачных объектов нормально)
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;                    // ВКЛЮЧАЕМ проверку глубины
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // НЕ пишем в depth buffer
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;     // Стандартное сравнение (ближе -> видимо)
    dsDesc.StencilEnable = false;

    game->Device->CreateDepthStencilState(&dsDesc, &depthState);
}

void ParticleEmitter::Initialize() {
    if (initialized) return;
    if (!game || !game->Device) return;

    particles.resize(maxParticles);
    currentParticleCount = 0;

    CreateGeometryBuffers();
    CreateShaders();
    CreateStates();

    initialized = true;
}

void ParticleEmitter::EmitParticle() {
    if (currentParticleCount >= maxParticles) return;

    Particle& p = particles[currentParticleCount];

    // Позиция с небольшим разбросом
    p.position = emitterPosition;
    p.position.x += (dist(rng) - 0.5f) * 0.5f;
    p.position.z += (dist(rng) - 0.5f) * 0.5f;

    // Скорость
    float speed = particleSpeedMin + dist(rng) * (particleSpeedMax - particleSpeedMin);

    // Направление - конус вверх
    float angleH = (dist(rng) - 0.5f) * 0.8f;   // горизонтальный разброс
    float angleV = 0.5f + dist(rng) * 0.8f;     // вертикальный угол (0.5-1.3 рад)

    Vector3 dir;
    dir.x = sin(angleH) * cos(angleV);
    dir.y = sin(angleV);
    dir.z = cos(angleH) * cos(angleV);
    dir.Normalize();

    // Смешиваем с направлением эмиттера
    dir = (emitterDirection + dir * 0.6f);
    dir.Normalize();

    p.velocity = dir * speed;
    p.acceleration = useGravity ? gravity : Vector3::Zero;

    p.maxLife = particleLifeMin + dist(rng) * (particleLifeMax - particleLifeMin);
    p.life = p.maxLife;

    p.size = particleSizeMin + dist(rng) * (particleSizeMax - particleSizeMin);
    p.color = startColor;

    currentParticleCount++;
}

void ParticleEmitter::Update(float deltaTime) {
    if (!initialized) return;

    // Эмиссия
    timeSinceLastEmission += deltaTime;
    float timePerParticle = 1.0f / particlesPerSecond;

    while (timeSinceLastEmission >= timePerParticle && currentParticleCount < maxParticles) {
        EmitParticle();
        timeSinceLastEmission -= timePerParticle;
    }

    // Обновление частиц
    for (int i = 0; i < currentParticleCount; i++) {
        Particle& p = particles[i];

        p.life -= deltaTime;

        if (p.life <= 0) {
            // Удаляем
            if (i < currentParticleCount - 1) {
                particles[i] = particles[currentParticleCount - 1];
            }
            currentParticleCount--;
            i--;
            continue;
        }

        // Физика
        p.velocity += p.acceleration * deltaTime;
        p.position += p.velocity * deltaTime;

        // Отскок от пола
        if (p.position.y <= groundY) {
            p.position.y = groundY;
            p.velocity.y = -p.velocity.y * bounceDamping;

            if (std::abs(p.velocity.y) < 0.5f) {
                p.velocity.y = 0;
            }
        }

        // Интерполяция цвета
        float t = 1.0f - (p.life / p.maxLife);
        p.color = Vector4::Lerp(startColor, endColor, t);
    }
}

void ParticleEmitter::UpdateVertexBuffer() {
    if (!vertexBuffer || currentParticleCount == 0) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = game->Context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    ParticleVertex* vertices = (ParticleVertex*)mapped.pData;

    // Получаем направления камеры для билбордов
    Vector3 camRight, camUp;
    if (game->Camera) {
        Vector3 camPos = game->Camera->GetPosition();
        Vector3 camForward = game->Camera->GetForward();
        camRight = camForward.Cross(Vector3(0, 1, 0));
        camRight.Normalize();
        camUp = camRight.Cross(camForward);
        camUp.Normalize();
    }
    else {
        camRight = Vector3(1, 0, 0);
        camUp = Vector3(0, 1, 0);
    }

    for (int i = 0; i < currentParticleCount; i++) {
        const Particle& p = particles[i];
        int baseIdx = i * 4;

        Vector3 center = p.position;
        Vector3 halfRight = camRight * p.size * 0.5f;
        Vector3 halfUp = camUp * p.size * 0.5f;

        // 4 вершины квадрата (билборд)
        vertices[baseIdx + 0].position = center - halfRight - halfUp;
        vertices[baseIdx + 0].color = p.color;

        vertices[baseIdx + 1].position = center + halfRight - halfUp;
        vertices[baseIdx + 1].color = p.color;

        vertices[baseIdx + 2].position = center + halfRight + halfUp;
        vertices[baseIdx + 2].color = p.color;

        vertices[baseIdx + 3].position = center - halfRight + halfUp;
        vertices[baseIdx + 3].color = p.color;
    }

    game->Context->Unmap(vertexBuffer, 0);
}

void ParticleEmitter::Draw() {
    if (!initialized || !game || !game->Context || currentParticleCount == 0) return;

    UpdateVertexBuffer();

    // Сохраняем текущее состояние
    ID3D11BlendState* oldBlendState = nullptr;
    float oldBlendFactor[4];
    UINT oldSampleMask;
    game->Context->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);

    ID3D11DepthStencilState* oldDepthState = nullptr;
    UINT oldStencilRef;
    game->Context->OMGetDepthStencilState(&oldDepthState, &oldStencilRef);

    // Устанавливаем состояние для частиц
    float blendFactor[4] = { 0, 0, 0, 0 };
    game->Context->OMSetBlendState(additiveBlendState, blendFactor, 0xffffffff);
    game->Context->OMSetDepthStencilState(depthState, 0);

    // Матрицы
    Matrix view = game->Camera->GetViewMatrix();
    Matrix proj = game->Camera->GetProjectionMatrix();

    struct VSCB {
        Matrix view;
        Matrix projection;
    } vsData;
    vsData.view = view.Transpose();
    vsData.projection = proj.Transpose();

    game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsData, 0, 0);

    // Отрисовка
    UINT stride = sizeof(ParticleVertex);
    UINT offset = 0;
    game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    game->Context->IASetInputLayout(inputLayout);
    game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    game->Context->VSSetShader(vertexShader, nullptr, 0);
    game->Context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);

    game->Context->PSSetShader(pixelShader, nullptr, 0);

    game->Context->DrawIndexed(currentParticleCount * 6, 0, 0);

    // Восстанавливаем состояние
    game->Context->OMSetBlendState(oldBlendState, oldBlendFactor, oldSampleMask);
    game->Context->OMSetDepthStencilState(oldDepthState, oldStencilRef);

    if (oldBlendState) oldBlendState->Release();
    if (oldDepthState) oldDepthState->Release();
}

void ParticleEmitter::DrawGeometry(RenderingSystem* rs) {
    // Частицы не участвуют в G-Buffer, рисуются в forward pass
    (void)rs;
}

void ParticleEmitter::DrawShadow() {
    // Частицы не отбрасывают тени
}

void ParticleEmitter::DestroyResources() {
    if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
    if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
    if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
    if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
    if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
    if (vsConstantBuffer) { vsConstantBuffer->Release(); vsConstantBuffer = nullptr; }
    if (additiveBlendState) { additiveBlendState->Release(); additiveBlendState = nullptr; }
    if (depthState) { depthState->Release(); depthState = nullptr; }

    initialized = false;
}

void ParticleEmitter::SetSpeedRange(float minSpeed, float maxSpeed) {
    particleSpeedMin = minSpeed;
    particleSpeedMax = maxSpeed;
}

void ParticleEmitter::SetLifeRange(float minLife, float maxLife) {
    particleLifeMin = minLife;
    particleLifeMax = maxLife;
}

void ParticleEmitter::SetSizeRange(float minSize, float maxSize) {
    particleSizeMin = minSize;
    particleSizeMax = maxSize;
}

void ParticleEmitter::SetColors(const Vector4& start, const Vector4& end) {
    startColor = start;
    endColor = end;
}

void ParticleEmitter::SetDirection(const Vector3& dir) {
    emitterDirection = dir;
    emitterDirection.Normalize();
}

void ParticleEmitter::SetGravity(const Vector3& grav) {
    gravity = grav;
    useGravity = (gravity.LengthSquared() > 0);
}

void ParticleEmitter::SetGroundCollision(float y, float damping) {
    groundY = y;
    bounceDamping = damping;
}

void ParticleEmitter::SetupFountain(const Vector3& pos, const Vector4& color) {
    emitterPosition = pos;
    emitterDirection = Vector3(0, 1, 0);

    SetEmissionRate(70);
    SetMaxParticles(800);
    SetSpeedRange(4.0f, 9.0f);
    SetLifeRange(1.0f, 2.2f);
    SetSizeRange(0.1f, 0.25f);

    // Цвета: от заданного к прозрачному с оттенком
    SetColors(color, Vector4(color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 0.0f));

    SetGravity(Vector3(0, -13.0f, 0));
    SetGroundCollision(0.0f, 0.45f);
}