#include "KatamariBallGameComponent.h"
#include "OrbitalCameraGameComponent.h"
#include "PropGameComponent.h"
#include "Game.h"
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

KatamariBallGameComponent::KatamariBallGameComponent(Game* game, OrbitalCameraGameComponent* cam,
    const Vector3& startPos, float startRadius)
    : GameComponent(game)
    , position(startPos)
    , radius(startRadius)
    , targetRadius(startRadius)
    , velocity(Vector3::Zero)
    , moveSpeed(5.0f)
    , rotationAngle(0)
    , camera(cam)
    , growAnimationTime(0)
    , ballColor(0.2f, 0.7f, 0.2f, 1.0f)
    , sphereInitialized(false)
    , debugVertexBuffer(nullptr)
    , debugIndexBuffer(nullptr)
    , debugVS(nullptr)
    , debugPS(nullptr)
    , debugLayout(nullptr)
    , debugConstantBuffer(nullptr)
    , debugInitialized(false)
{
}

KatamariBallGameComponent::~KatamariBallGameComponent() {
    DestroyResources();
}

void KatamariBallGameComponent::Initialize() {
    if (!sphereInitialized && game && game->Device) {
        sphereRenderer.Initialize(game, ballColor, 48);
        sphereInitialized = true;
        InitDebugCollider();
        std::cout << "[Ball] Initialized! Radius: " << radius << std::endl;
    }
}

void KatamariBallGameComponent::InitDebugCollider() {
    if (debugInitialized || !game || !game->Device) return;

    const char* vsCode = R"(
        cbuffer ConstantBuffer : register(b0) {
            float4x4 viewProj;
        };
        
        struct VSInput {
            float3 position : POSITION;
            float4 color : COLOR;
        };
        
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };
        
        VSOutput VSMain(VSInput input) {
            VSOutput output;
            output.position = mul(float4(input.position, 1.0f), viewProj);
            output.color = input.color;
            return output;
        }
    )";

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* error = nullptr;

    HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
        "VSMain", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, &error);

    if (FAILED(hr)) {
        if (error) error->Release();
        return;
    }

    const char* psCode = R"(
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };
        
        float4 PSMain(VSOutput input) : SV_TARGET {
            return input.color;
        }
    )";

    ID3DBlob* psBlob = nullptr;
    hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr,
        "PSMain", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, &error);

    if (FAILED(hr)) {
        if (error) error->Release();
        vsBlob->Release();
        return;
    }

    game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &debugVS);
    game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &debugPS);

    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    game->Device->CreateInputLayout(elements, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &debugLayout);

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(Matrix);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    game->Device->CreateBuffer(&cbDesc, nullptr, &debugConstantBuffer);

    vsBlob->Release();
    psBlob->Release();

    debugInitialized = true;
}

void KatamariBallGameComponent::DrawDebugCollider() {
    if (!debugInitialized || !game || !game->Context || !game->Camera) return;

    struct DebugVertex { Vector3 position; Vector4 color; };
    std::vector<DebugVertex> vertices;
    std::vector<UINT> indices;

    int segments = 24;
    float angleStep = 2.0f * 3.14159f / segments;

    // XZ plane (horizontal)
    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;
        vertices.push_back({ Vector3(position.x + x, position.y, position.z + z), Vector4(0, 1, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    int xzStart = vertices.size() - (segments + 1);

    // XY plane (vertical)
    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float x = cos(angle) * radius;
        float y = sin(angle) * radius;
        vertices.push_back({ Vector3(position.x + x, position.y + y, position.z), Vector4(0, 1, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(xzStart + segments + 1 + i);
        indices.push_back(xzStart + segments + 1 + i + 1);
    }

    int xyStart = xzStart + segments + 1;

    // YZ plane
    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float y = cos(angle) * radius;
        float z = sin(angle) * radius;
        vertices.push_back({ Vector3(position.x, position.y + y, position.z + z), Vector4(0, 1, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(xyStart + segments + 1 + i);
        indices.push_back(xyStart + segments + 1 + i + 1);
    }

    // Update vertex buffer
    if (debugVertexBuffer) { debugVertexBuffer->Release(); debugVertexBuffer = nullptr; }
    if (debugIndexBuffer) { debugIndexBuffer->Release(); debugIndexBuffer = nullptr; }

    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexDesc.ByteWidth = sizeof(DebugVertex) * (UINT)vertices.size();
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = { vertices.data() };
    game->Device->CreateBuffer(&vertexDesc, &vertexData, &debugVertexBuffer);

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_DEFAULT;
    indexDesc.ByteWidth = sizeof(UINT) * (UINT)indices.size();
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = { indices.data() };
    game->Device->CreateBuffer(&indexDesc, &indexData, &debugIndexBuffer);

    Matrix viewProj = game->Camera->GetViewMatrix() * game->Camera->GetProjectionMatrix();
    Matrix transposed = viewProj.Transpose();
    game->Context->UpdateSubresource(debugConstantBuffer, 0, nullptr, &transposed, 0, 0);

    UINT stride = sizeof(DebugVertex);
    UINT offset = 0;
    game->Context->IASetVertexBuffers(0, 1, &debugVertexBuffer, &stride, &offset);
    game->Context->IASetIndexBuffer(debugIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    game->Context->IASetInputLayout(debugLayout);
    game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    game->Context->VSSetShader(debugVS, nullptr, 0);
    game->Context->VSSetConstantBuffers(0, 1, &debugConstantBuffer);
    game->Context->PSSetShader(debugPS, nullptr, 0);

    game->Context->DrawIndexed((UINT)indices.size(), 0, 0);
}

void KatamariBallGameComponent::Update(float deltaTime) {
    ProcessInput(deltaTime);

    position += velocity * deltaTime;

    if (position.y < radius) {
        position.y = radius;
    }

    if (camera) {
        camera->SetTarget(position);
    }

    float speed = velocity.Length();
    if (speed > 0.01f) {
        rotationAngle += speed * deltaTime * 3.0f;
    }

    if (growAnimationTime > 0) {
        growAnimationTime -= deltaTime;
        float t = 1.0f - (growAnimationTime / 0.3f);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        radius = radius * (1 - t) + targetRadius * t;

        if (growAnimationTime <= 0) {
            radius = targetRadius;
            std::cout << "[Ball] Growth complete! New radius: " << radius << std::endl;
        }
    }

    UpdateAttachedObjects(deltaTime);
}

void KatamariBallGameComponent::ProcessInput(float deltaTime) {
    if (!game || !game->Input) return;

    Vector3 inputDir = Vector3::Zero;

    if (game->Input->IsKeyDown(Keys::W)) inputDir.z += 1;
    if (game->Input->IsKeyDown(Keys::S)) inputDir.z -= 1;
    if (game->Input->IsKeyDown(Keys::D)) inputDir.x += 1;
    if (game->Input->IsKeyDown(Keys::A)) inputDir.x -= 1;

    if (inputDir.LengthSquared() > 0) {
        inputDir.Normalize();

        if (camera) {
            Vector3 forward = camera->GetForward();
            Vector3 up(0, 1, 0);
            Vector3 right = forward.Cross(up);
            right.Normalize();
            forward = up.Cross(right);
            forward.Normalize();

            forward.y = 0;
            right.y = 0;
            forward.Normalize();
            right.Normalize();

            velocity = (forward * inputDir.z + right * inputDir.x) * moveSpeed;
        }
        else {
            velocity = inputDir * moveSpeed;
        }
    }
    else {
        velocity *= 0.95f;
    }
}

void KatamariBallGameComponent::CheckCollisions(std::vector<GameComponent*>& props) {
    for (auto* comp : props) {
        PropGameComponent* prop = dynamic_cast<PropGameComponent*>(comp);
        if (!prop) continue;
        if (attachedSet.find(prop) != attachedSet.end()) continue;

        Vector3 ballCenter = position;
        Vector3 propCenter = prop->GetCollisionCenter();
        float propRadius = prop->GetCollisionRadius();

        float dx = ballCenter.x - propCenter.x;
        float dy = ballCenter.y - propCenter.y;
        float dz = ballCenter.z - propCenter.z;
        float distance = sqrt(dx * dx + dy * dy + dz * dz);

        float collisionDistance = radius + propRadius;

        // Отладка - выводим каждый кадр для ближайших объектов
        if (distance < 2.0f) {
            std::cout << "[COLLISION CHECK] Ball=(" << ballCenter.x << "," << ballCenter.y << "," << ballCenter.z << ")"
                << " Prop=(" << propCenter.x << "," << propCenter.y << "," << propCenter.z << ")"
                << " Dist=" << distance << " Need=" << collisionDistance
                << " BallR=" << radius << " PropR=" << propRadius << std::endl;
        }

        if (distance < collisionDistance) {
            std::cout << "!!! COLLISION DETECTED !!! Distance: " << distance << " < " << collisionDistance << std::endl;
            AttachProp(prop);
        }
    }
}

bool KatamariBallGameComponent::AttachProp(PropGameComponent* prop) {
    if (!prop) return false;
    if (attachedSet.find(prop) != attachedSet.end()) return false;

    Vector3 propPos = prop->GetPosition();
    Vector3 direction = propPos - position;

    if (direction.Length() > 0.001f) {
        direction.Normalize();
    }
    else {
        direction = Vector3(1, 0, 0);
    }

    Vector3 surfacePos = position + direction * radius;
    Vector3 relativePos = surfacePos - position;
    Vector3 originalScale = prop->GetModel().GetScale();

    attachedObjects.push_back(AttachedObject(prop, relativePos, originalScale));
    attachedSet.insert(prop);

    prop->SetPosition(surfacePos);

    float avgScale = (originalScale.x + originalScale.y + originalScale.z) / 3.0f;
    float radiusIncrease = 0.05f + avgScale * 0.1f;
    float newRadius = radius + radiusIncrease;

    targetRadius = newRadius;
    growAnimationTime = 0.3f;

    float colorFactor = 1.0f + (attachedObjects.size() * 0.015f);
    if (colorFactor > 2.0f) colorFactor = 2.0f;
    ballColor = Vector4(0.2f * colorFactor, 0.7f, 0.2f, 1.0f);

    std::cout << "[Ball] Attached! Total: " << attachedObjects.size()
        << ", Radius: " << radius << " -> " << targetRadius << std::endl;

    return true;
}

void KatamariBallGameComponent::UpdateAttachedObjects(float deltaTime) {
    for (auto& attached : attachedObjects) {
        if (attached.prop) {
            Vector3 direction = attached.relativePosition;
            if (direction.Length() > 0.001f) {
                direction.Normalize();
            }
            else {
                direction = Vector3(1, 0, 0);
            }

            Vector3 newPos = position + direction * radius;
            attached.prop->SetPosition(newPos);
            attached.relativePosition = direction * radius;

            attached.attachmentTime += deltaTime;
            float rotY = attached.attachmentTime * 0.8f;
            attached.prop->GetModel().SetRotation(Vector3(rotY * 0.5f, rotY, rotY * 0.3f));
        }
    }
}

void KatamariBallGameComponent::Draw() {
    DrawBall();
    DrawDebugCollider();
}

void KatamariBallGameComponent::DrawBall() {
    if (!game || !game->Camera || !sphereInitialized) return;

    Matrix world = Matrix::CreateScale(radius) *
        Matrix::CreateRotationY(rotationAngle) *
        Matrix::CreateTranslation(position);

    sphereRenderer.Draw(game, world, ballColor,
        game->Camera->GetViewMatrix(),
        game->Camera->GetProjectionMatrix());
}

void KatamariBallGameComponent::DestroyResources() {
    if (sphereInitialized) {
        sphereRenderer.DestroyResources();
        sphereInitialized = false;
    }

    if (debugVertexBuffer) { debugVertexBuffer->Release(); debugVertexBuffer = nullptr; }
    if (debugIndexBuffer) { debugIndexBuffer->Release(); debugIndexBuffer = nullptr; }
    if (debugVS) { debugVS->Release(); debugVS = nullptr; }
    if (debugPS) { debugPS->Release(); debugPS = nullptr; }
    if (debugLayout) { debugLayout->Release(); debugLayout = nullptr; }
    if (debugConstantBuffer) { debugConstantBuffer->Release(); debugConstantBuffer = nullptr; }

    debugInitialized = false;
}