#include "KatamariBall.h"
#include "Prop.h"
#include "Game.h"
#include <d3dcompiler.h>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

KatamariBall::KatamariBall(Game* game, OrbitalCamera* cam,
    const Vector3& startPos, float startRadius, const std::string& textureFile)
    : GameComponent(game), position(startPos), radius(startRadius), targetRadius(startRadius),
    camera(cam), texturePath(textureFile) {
}

KatamariBall::~KatamariBall() { DestroyResources(); }

ID3DBlob* KatamariBall::CompileDebugShader(const char* code, const char* target, const char* entryPoint) {
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

void KatamariBall::Initialize() {
    if (!sphereInitialized && game && game->Device) {
        sphereRenderer.Initialize(game, ballColor, 48);
        sphereInitialized = true;
        InitDebugCollider();

        if (!texturePath.empty()) {
            ballTexture = Core::TextureLoader::LoadTexture2D(game, texturePath, true);
        }
    }
}

void KatamariBall::InitDebugCollider() {
    if (debugInitialized || !game || !game->Device) return;

    const char* vsCode = R"(
        cbuffer ConstantBuffer : register(b0) { float4x4 viewProj; };
        struct VSInput { float3 position : POSITION; float4 color : COLOR; };
        struct VSOutput { float4 position : SV_POSITION; float4 color : COLOR; };
        VSOutput VSMain(VSInput input) {
            VSOutput output;
            output.position = mul(float4(input.position, 1.0f), viewProj);
            output.color = input.color;
            return output;
        }
    )";

    const char* psCode = R"(
        struct VSOutput { float4 position : SV_POSITION; float4 color : COLOR; };
        float4 PSMain(VSOutput input) : SV_TARGET { return input.color; }
    )";

    ID3DBlob* vsBlob = CompileDebugShader(vsCode, "vs_5_0", "VSMain");
    ID3DBlob* psBlob = CompileDebugShader(psCode, "ps_5_0", "PSMain");

    if (!vsBlob || !psBlob) {
        if (vsBlob) vsBlob->Release();
        if (psBlob) psBlob->Release();
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

void KatamariBall::DrawDebugCollider() {
    if (!debugInitialized || !game || !game->Context || !game->Camera) return;

    struct DebugVertex { Vector3 position; Vector4 color; };
    std::vector<DebugVertex> vertices;
    std::vector<UINT> indices;

    const int SEGMENTS = 24;
    const float ANGLE_STEP = XM_2PI / SEGMENTS;

    // XZ circle
    for (int i = 0; i <= SEGMENTS; i++) {
        float angle = i * ANGLE_STEP;
        vertices.push_back({ Vector3(position.x + cos(angle) * radius, position.y, position.z + sin(angle) * radius), Vector4(0, 1, 0, 1) });
        if (i < SEGMENTS) { indices.push_back(i); indices.push_back(i + 1); }
    }

    // XY circle
    int xyStart = (int)vertices.size();
    for (int i = 0; i <= SEGMENTS; i++) {
        float angle = i * ANGLE_STEP;
        vertices.push_back({ Vector3(position.x + cos(angle) * radius, position.y + sin(angle) * radius, position.z), Vector4(0, 1, 0, 1) });
        if (i < SEGMENTS) { indices.push_back(xyStart + i); indices.push_back(xyStart + i + 1); }
    }

    // YZ circle
    int yzStart = (int)vertices.size();
    for (int i = 0; i <= SEGMENTS; i++) {
        float angle = i * ANGLE_STEP;
        vertices.push_back({ Vector3(position.x, position.y + cos(angle) * radius, position.z + sin(angle) * radius), Vector4(0, 1, 0, 1) });
        if (i < SEGMENTS) { indices.push_back(yzStart + i); indices.push_back(yzStart + i + 1); }
    }

    // Create buffers
    if (debugVertexBuffer) { debugVertexBuffer->Release(); debugVertexBuffer = nullptr; }
    if (debugIndexBuffer) { debugIndexBuffer->Release(); debugIndexBuffer = nullptr; }

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;

    desc.ByteWidth = sizeof(DebugVertex) * (UINT)vertices.size();
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data = { vertices.data() };
    game->Device->CreateBuffer(&desc, &data, &debugVertexBuffer);

    desc.ByteWidth = sizeof(UINT) * (UINT)indices.size();
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    data.pSysMem = indices.data();
    game->Device->CreateBuffer(&desc, &data, &debugIndexBuffer);

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

void KatamariBall::ProcessInput(float deltaTime) {
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

void KatamariBall::ProcessAttachedObjectInput(float deltaTime) {
    if (!game || !game->Input || attachedObjects.empty()) return;

    bool moving = false;
    if (game->Input->IsKeyDown(Keys::Left)) { attachedMoveAngleH -= 2.0f * deltaTime; moving = true; }
    if (game->Input->IsKeyDown(Keys::Right)) { attachedMoveAngleH += 2.0f * deltaTime; moving = true; }
    if (game->Input->IsKeyDown(Keys::Up)) { attachedMoveAngleV -= 2.0f * deltaTime; moving = true; }
    if (game->Input->IsKeyDown(Keys::Down)) { attachedMoveAngleV += 2.0f * deltaTime; moving = true; }

    if (moving) {
        while (attachedMoveAngleH > XM_2PI) attachedMoveAngleH -= XM_2PI;
        while (attachedMoveAngleH < 0) attachedMoveAngleH += XM_2PI;

        Vector3 newLocalDir;
        newLocalDir.x = cos(attachedMoveAngleV) * sin(attachedMoveAngleH);
        newLocalDir.y = sin(attachedMoveAngleV);
        newLocalDir.z = cos(attachedMoveAngleV) * cos(attachedMoveAngleH);
        newLocalDir.Normalize();

        attachedObjects.back().relativePosition = newLocalDir;
    }
}

void KatamariBall::Update(float deltaTime) {
    ProcessInput(deltaTime);
    ProcessAttachedObjectInput(deltaTime);

    position += velocity * deltaTime;
    if (position.y < radius) position.y = radius;

    if (camera) camera->SetTarget(position);

    float speed = velocity.Length();
    if (speed > 0.01f) {
        float distance = speed * deltaTime;
        float angleDelta = (distance / (XM_2PI * radius)) * XM_2PI;

        Vector3 moveDir = velocity;
        moveDir.y = 0;
        if (moveDir.Length() > 0.01f) {
            moveDir.Normalize();
            Vector3 rotAxis = Vector3(0, 1, 0).Cross(moveDir);
            rotAxis = (rotAxis.Length() < 0.1f) ? Vector3(1, 0, 0) : rotAxis;
            rotAxis.Normalize();
            rotationAngle += angleDelta;
            while (rotationAngle > XM_2PI) rotationAngle -= XM_2PI;
        }
    }

    if (growAnimationTime > 0) {
        growAnimationTime -= deltaTime;
        float t = 1.0f - (growAnimationTime / 0.3f);
        t = std::clamp(t, 0.0f, 1.0f);
        radius = radius * (1 - t) + targetRadius * t;
        if (growAnimationTime <= 0) radius = targetRadius;
    }

    UpdateAttachedObjects(deltaTime);
    CheckCollisions(props);
}

void KatamariBall::CheckCollisions(std::vector<GameComponent*>& props) {
    for (auto* comp : props) {
        Prop* prop = dynamic_cast<Prop*>(comp);
        if (!prop || attachedSet.find(prop) != attachedSet.end()) continue;

        Vector3 ballCenter = position;
        Vector3 propCenter = prop->GetCollisionCenter();
        float propRadius = prop->GetCollisionRadius();

        Vector3 diff = ballCenter - propCenter;
        float distanceSquared = diff.LengthSquared();
        float collisionDistance = radius + propRadius;

        if (distanceSquared < collisionDistance * collisionDistance) {
            AttachProp(prop);
        }
    }
}

bool KatamariBall::AttachProp(Prop* prop) {
    if (!prop || attachedSet.find(prop) != attachedSet.end()) return false;

    Vector3 propPos = prop->GetPosition();
    Vector3 direction = propPos - position;
    if (direction.Length() > 0.001f) direction.Normalize();
    else direction = Vector3(1, 0, 0);

    float propRadius = prop->GetCollisionRadius();
    float absoluteDepth = propRadius * 0.3f;
    float distanceToSurface = (propPos - position).Length() - radius;
    if (distanceToSurface < 0) {
        absoluteDepth = -distanceToSurface + propRadius * 0.2f;
    }
    float relativeDepth = absoluteDepth / radius;

    Vector3 surfacePos = position + direction * (radius - absoluteDepth);
    Vector3 originalScale = prop->GetModel().GetScale();

    attachedObjects.push_back(AttachedObject(prop, direction, relativeDepth, originalScale));
    attachedSet.insert(prop);

    prop->SetPosition(surfacePos);
    prop->GetModel().SetRotation(Vector3::Zero);

    float avgScale = (originalScale.x + originalScale.y + originalScale.z) / 3.0f;
    targetRadius = radius + 0.05f + avgScale * 0.1f;
    growAnimationTime = 0.3f;

    attachedMoveAngleH = 0;
    attachedMoveAngleV = 0;

    return true;
}

void KatamariBall::UpdateAttachedObjects(float deltaTime) {
    Vector3 rotAxis = Vector3(1, 0, 0);
    float speed = velocity.Length();
    if (speed > 0.01f) {
        Vector3 moveDir = velocity;
        moveDir.y = 0;
        if (moveDir.Length() > 0.01f) {
            moveDir.Normalize();
            rotAxis = Vector3(0, 1, 0).Cross(moveDir);
            rotAxis = (rotAxis.Length() < 0.1f) ? Vector3(1, 0, 0) : rotAxis;
            rotAxis.Normalize();
        }
    }

    Quaternion ballRotation = Quaternion::CreateFromAxisAngle(rotAxis, rotationAngle);

    for (auto& attached : attachedObjects) {
        if (attached.prop) {
            Vector3 localDir = attached.relativePosition;
            localDir.Normalize();
            Vector3 worldDir = Vector3::Transform(localDir, ballRotation);
            worldDir.Normalize();

            float attachRadius = radius - radius * attached.depth;
            if (attachRadius < 0.1f) attachRadius = 0.1f;

            attached.prop->SetPosition(position + worldDir * attachRadius);
            attached.prop->GetModel().SetRotation(ballRotation.ToEuler());
            attached.attachmentTime += deltaTime;
        }
    }
}

void KatamariBall::DrawBall() {
    if (!game || !game->Camera || !sphereInitialized) return;

    Vector3 rotAxis = Vector3(1, 0, 0);
    float speed = velocity.Length();
    if (speed > 0.01f) {
        Vector3 moveDir = velocity;
        moveDir.y = 0;
        if (moveDir.Length() > 0.01f) {
            moveDir.Normalize();
            rotAxis = Vector3(0, 1, 0).Cross(moveDir);
            rotAxis = (rotAxis.Length() < 0.1f) ? Vector3(1, 0, 0) : rotAxis;
            rotAxis.Normalize();
        }
    }

    Quaternion rotation = Quaternion::CreateFromAxisAngle(rotAxis, rotationAngle);
    Matrix world = Matrix::CreateScale(radius) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);

    Material ballMaterial(ballColor, 64.0f);
    ballMaterial.specular = Vector4(0.8f, 0.8f, 0.8f, 1.0f);

    sphereRenderer.Draw(game, world, ballColor,
        game->Camera->GetViewMatrix(),
        game->Camera->GetProjectionMatrix(),
        ballTexture, &ballMaterial, true);
}

void KatamariBall::Draw() {
    DrawBall();
    DrawDebugCollider();
}

void KatamariBall::DestroyResources() {
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

    if (ballTexture) { ballTexture->Release(); ballTexture = nullptr; }

    debugInitialized = false;
}