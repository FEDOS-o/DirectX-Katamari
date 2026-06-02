// Particle.cpp
#include "Particle.h"
#include "Game.h"
#include "Camera.h"
#include "RenderingSystem.h"
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
    , initialized(false)
    , restitution(0.65f)
    , friction(0.95f)
    , useGBufferCollision(true) {

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
            output.depth = viewPos.z;
            return output;
        }
    )";

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

    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    game->Device->CreateInputLayout(elements, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

    vsBlob->Release();
    psBlob->Release();

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(Matrix) * 2;
    game->Device->CreateBuffer(&cbDesc, nullptr, &vsConstantBuffer);
}

void ParticleEmitter::CreateStates() {
    if (!game || !game->Device) return;

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

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
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

    p.position = emitterPosition;
    p.position.x += (dist(rng) - 0.5f) * 0.5f;
    p.position.z += (dist(rng) - 0.5f) * 0.5f;

    float speed = particleSpeedMin + dist(rng) * (particleSpeedMax - particleSpeedMin);

    float angleH = (dist(rng) - 0.5f) * 0.8f;
    float angleV = 0.5f + dist(rng) * 0.8f;

    Vector3 dir;
    dir.x = sin(angleH) * cos(angleV);
    dir.y = sin(angleV);
    dir.z = cos(angleH) * cos(angleV);
    dir.Normalize();

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

void ParticleEmitter::UpdateParticlesWithGBuffer(float deltaTime) {
    if (!game->renderingSystem || !game->renderingSystem->GetGBuffer()) {
        UpdateParticlesSimple(deltaTime);
        return;
    }

    GBuffer* gbuffer = game->renderingSystem->GetGBuffer();

    ID3D11ShaderResourceView* depthSRV = gbuffer->GetDepthSRV();
    ID3D11ShaderResourceView* normalSRV = gbuffer->GetSRV(GBuffer::NORMAL);

    if (!depthSRV || !normalSRV) {
        UpdateParticlesSimple(deltaTime);
        return;
    }

    ID3D11Texture2D* depthTexture = nullptr;
    ID3D11Texture2D* normalTexture = nullptr;
    depthSRV->GetResource((ID3D11Resource**)&depthTexture);
    normalSRV->GetResource((ID3D11Resource**)&normalTexture);

    if (!depthTexture || !normalTexture) {
        if (depthTexture) depthTexture->Release();
        if (normalTexture) normalTexture->Release();
        UpdateParticlesSimple(deltaTime);
        return;
    }

    D3D11_TEXTURE2D_DESC depthDesc, normalDesc;
    depthTexture->GetDesc(&depthDesc);
    normalTexture->GetDesc(&normalDesc);

    int width = depthDesc.Width;
    int height = depthDesc.Height;

    ID3D11Texture2D* stagingDepth = nullptr;
    D3D11_TEXTURE2D_DESC stagingDesc = depthDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = game->Device->CreateTexture2D(&stagingDesc, nullptr, &stagingDepth);
    if (FAILED(hr)) {
        depthTexture->Release();
        normalTexture->Release();
        UpdateParticlesSimple(deltaTime);
        return;
    }

    ID3D11Texture2D* stagingNormal = nullptr;
    stagingDesc = normalDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = game->Device->CreateTexture2D(&stagingDesc, nullptr, &stagingNormal);
    if (FAILED(hr)) {
        stagingDepth->Release();
        depthTexture->Release();
        normalTexture->Release();
        UpdateParticlesSimple(deltaTime);
        return;
    }

    game->Context->CopyResource(stagingDepth, depthTexture);
    game->Context->CopyResource(stagingNormal, normalTexture);

    D3D11_MAPPED_SUBRESOURCE mappedDepth, mappedNormal;
    hr = game->Context->Map(stagingDepth, 0, D3D11_MAP_READ, 0, &mappedDepth);
    if (FAILED(hr)) {
        stagingDepth->Release();
        stagingNormal->Release();
        depthTexture->Release();
        normalTexture->Release();
        UpdateParticlesSimple(deltaTime);
        return;
    }

    hr = game->Context->Map(stagingNormal, 0, D3D11_MAP_READ, 0, &mappedNormal);
    if (FAILED(hr)) {
        game->Context->Unmap(stagingDepth, 0);
        stagingDepth->Release();
        stagingNormal->Release();
        depthTexture->Release();
        normalTexture->Release();
        UpdateParticlesSimple(deltaTime);
        return;
    }

    float* depthData = (float*)mappedDepth.pData;
    uint16_t* normalDataHalf = (uint16_t*)mappedNormal.pData;

    auto HalfToFloat = [](uint16_t half) -> float {
        unsigned int sign = (half >> 15) & 0x1;
        unsigned int exponent = (half >> 10) & 0x1F;
        unsigned int mantissa = half & 0x3FF;

        if (exponent == 0) {
            if (mantissa == 0) return 0.0f;
            float result = (float)mantissa / 1024.0f;
            result *= powf(2.0f, -14.0f);
            return sign ? -result : result;
        }
        else if (exponent == 31) {
            return sign ? -INFINITY : INFINITY;
        }

        int floatExp = exponent - 15 + 127;
        unsigned int floatBits = (sign << 31) | (floatExp << 23) | (mantissa << 13);
        return *(float*)&floatBits;
        };

    Matrix viewProj = game->Camera->GetViewMatrix() * game->Camera->GetProjectionMatrix();
    Matrix invViewProj = viewProj.Invert();

    for (int i = 0; i < currentParticleCount; i++) {
        Particle& p = particles[i];

        p.life -= deltaTime;

        if (p.life <= 0) {
            if (i < currentParticleCount - 1) {
                particles[i] = particles[currentParticleCount - 1];
            }
            currentParticleCount--;
            i--;
            continue;
        }

        Vector3 oldPos = p.position;
        p.velocity += p.acceleration * deltaTime;
        Vector3 newPos = p.position + p.velocity * deltaTime;

        // SWEPT COLLISION: проверяем несколько точек на линии движения
        Vector3 dir = newPos - oldPos;
        float dist = dir.Length();

        if (dist > 0.01f) {
            dir /= dist;

            // Количество шагов зависит от скорости и размера частицы
            int steps = std::max(5, (int)(dist / (p.size * 0.3f)) + 1);
            steps = std::min(steps, 30); // Не больше 30 шагов

            bool collision = false;
            Vector3 hitNormal(0, 1, 0);
            Vector3 hitPos;
            float hitT = 1.0f;

            // Проверяем каждую точку на линии
            for (int step = 1; step <= steps; step++) {
                float t = (float)step / steps;
                Vector3 checkPos = oldPos + dir * (dist * t);

                Vector4 clipPos = Vector4::Transform(Vector4(checkPos.x, checkPos.y, checkPos.z, 1.0f), viewProj);

                if (clipPos.w > 0) {
                    Vector3 ndc = Vector3(clipPos.x / clipPos.w, clipPos.y / clipPos.w, clipPos.z / clipPos.w);

                    int texX = (int)((ndc.x * 0.5f + 0.5f) * width);
                    int texY = (int)((1.0f - (ndc.y * 0.5f + 0.5f)) * height);

                    texX = std::clamp(texX, 0, width - 1);
                    texY = std::clamp(texY, 0, height - 1);

                    int depthPitch = mappedDepth.RowPitch / sizeof(float);
                    int depthIdx = texY * depthPitch + texX;

                    if (depthIdx >= 0 && depthIdx < (width * height * 4)) {
                        float sceneDepth = depthData[depthIdx];
                        float particleDepth = clipPos.z / clipPos.w;

                        if (sceneDepth < 0.999f && sceneDepth > 0 && particleDepth > sceneDepth) {
                            collision = true;
                            hitT = t;

                            // Получаем позицию и нормаль в точке удара
                            Vector3 ndcSurface(ndc.x, ndc.y, sceneDepth);
                            Vector4 clipSurfacePos(ndcSurface.x * 2.0f - 1.0f,
                                1.0f - ndcSurface.y * 2.0f,
                                ndcSurface.z, 1.0f);
                            Vector4 worldSurfacePos = Vector4::Transform(clipSurfacePos, invViewProj);

                            if (worldSurfacePos.w != 0) {
                                hitPos = Vector3(worldSurfacePos.x / worldSurfacePos.w,
                                    worldSurfacePos.y / worldSurfacePos.w,
                                    worldSurfacePos.z / worldSurfacePos.w);

                                int normalPitch = mappedNormal.RowPitch / sizeof(uint16_t);
                                int normalBaseIdx = texY * normalPitch + texX * 4;

                                hitNormal = Vector3(
                                    HalfToFloat(normalDataHalf[normalBaseIdx + 0]) * 2.0f - 1.0f,
                                    HalfToFloat(normalDataHalf[normalBaseIdx + 1]) * 2.0f - 1.0f,
                                    HalfToFloat(normalDataHalf[normalBaseIdx + 2]) * 2.0f - 1.0f
                                );
                                hitNormal.Normalize();
                            }
                            break;
                        }
                    }
                }
            }

            if (collision) {
                float particleRadius = p.size * 0.5f;

                // Позиция в момент удара
                Vector3 collisionPos = oldPos + dir * (dist * hitT);
                p.position = hitPos + hitNormal * (particleRadius + 0.05f);

                // Отскок с учетом скорости в момент удара
                Vector3 velAtCollision = p.velocity;
                float speedAlongNormal = velAtCollision.Dot(hitNormal);

                if (speedAlongNormal < 0) {
                    // Сильный отскок
                    float bounceStrength = restitution * 1.3f;
                    Vector3 newVelocity = velAtCollision - hitNormal * speedAlongNormal * (1.0f + bounceStrength);

                    // Добавляем случайность
                    newVelocity.x += (this->dist(rng) - 0.5f) * 1.5f;
                    newVelocity.z += (this->dist(rng) - 0.5f) * 1.5f;

                    Vector3 normalComp = hitNormal * newVelocity.Dot(hitNormal);
                    Vector3 tangentComp = newVelocity - normalComp;
                    p.velocity = normalComp + tangentComp * friction;

                    // Минимальный импульс чтобы не залипало
                    if (p.velocity.Length() < 1.0f) {
                        p.velocity = hitNormal * 3.0f;
                    }
                }
            }
            else {
                p.position = newPos;
            }
        }
        else {
            p.position = newPos;
        }

        // Fallback для пола
        if (p.position.y <= groundY) {
            p.position.y = groundY + p.size * 0.5f;
            if (p.velocity.y < 0) {
                p.velocity.y = -p.velocity.y * bounceDamping * 1.2f;
            }
            if (std::abs(p.velocity.y) < 1.0f) {
                p.velocity.y = 3.0f;
            }
        }

        float t = 1.0f - (p.life / p.maxLife);
        p.color = Vector4::Lerp(startColor, endColor, t);
    }

    game->Context->Unmap(stagingDepth, 0);
    game->Context->Unmap(stagingNormal, 0);

    stagingDepth->Release();
    stagingNormal->Release();
    depthTexture->Release();
    normalTexture->Release();
}

void ParticleEmitter::UpdateParticlesSimple(float deltaTime) {
    for (int i = 0; i < currentParticleCount; i++) {
        Particle& p = particles[i];

        p.life -= deltaTime;

        if (p.life <= 0) {
            if (i < currentParticleCount - 1) {
                particles[i] = particles[currentParticleCount - 1];
            }
            currentParticleCount--;
            i--;
            continue;
        }

        p.velocity += p.acceleration * deltaTime;
        p.position += p.velocity * deltaTime;

        if (p.position.y <= groundY) {
            p.position.y = groundY;
            p.velocity.y = -p.velocity.y * bounceDamping;

            if (std::abs(p.velocity.y) < 0.5f) {
                p.velocity.y = 0;
            }
        }

        float t = 1.0f - (p.life / p.maxLife);
        p.color = Vector4::Lerp(startColor, endColor, t);
    }
}

void ParticleEmitter::Update(float deltaTime) {
    if (!initialized) return;

    timeSinceLastEmission += deltaTime;
    float timePerParticle = 1.0f / particlesPerSecond;

    while (timeSinceLastEmission >= timePerParticle && currentParticleCount < maxParticles) {
        EmitParticle();
        timeSinceLastEmission -= timePerParticle;
    }

    if (useGBufferCollision && game->renderingSystem && game->renderingSystem->GetGBuffer()) {
        UpdateParticlesWithGBuffer(deltaTime);
    }
    else {
        UpdateParticlesSimple(deltaTime);
    }
}

void ParticleEmitter::UpdateVertexBuffer() {
    if (!vertexBuffer || currentParticleCount == 0) return;

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = game->Context->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    ParticleVertex* vertices = (ParticleVertex*)mapped.pData;

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

    ID3D11BlendState* oldBlendState = nullptr;
    float oldBlendFactor[4];
    UINT oldSampleMask;
    game->Context->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);

    ID3D11DepthStencilState* oldDepthState = nullptr;
    UINT oldStencilRef;
    game->Context->OMGetDepthStencilState(&oldDepthState, &oldStencilRef);

    float blendFactor[4] = { 0, 0, 0, 0 };
    game->Context->OMSetBlendState(additiveBlendState, blendFactor, 0xffffffff);
    game->Context->OMSetDepthStencilState(depthState, 0);

    Matrix view = game->Camera->GetViewMatrix();
    Matrix proj = game->Camera->GetProjectionMatrix();

    struct VSCB {
        Matrix view;
        Matrix projection;
    } vsData;
    vsData.view = view.Transpose();
    vsData.projection = proj.Transpose();

    game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsData, 0, 0);

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

    game->Context->OMSetBlendState(oldBlendState, oldBlendFactor, oldSampleMask);
    game->Context->OMSetDepthStencilState(oldDepthState, oldStencilRef);

    if (oldBlendState) oldBlendState->Release();
    if (oldDepthState) oldDepthState->Release();
}

void ParticleEmitter::DrawGeometry(RenderingSystem* rs) {
    (void)rs;
}

void ParticleEmitter::DrawShadow() {
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
    SetSpeedRange(5.0f, 12.0f);      // Увеличенная скорость
    SetLifeRange(1.5f, 3.0f);        // Дольше живут
    SetSizeRange(0.1f, 0.3f);

    SetColors(color, Vector4(color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 0.0f));

    SetGravity(Vector3(0, -10.0f, 0)); // Чуть слабее гравитация
    SetGroundCollision(0.0f, 0.7f);

    useGBufferCollision = true;
    restitution = 0.85f;   // Высокая упругость - сильно отскакивают
    friction = 0.92f;      // Малое трение
}