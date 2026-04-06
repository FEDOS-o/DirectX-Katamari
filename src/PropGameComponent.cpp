#include "PropGameComponent.h"
#include "Game.h"
#include <d3dcompiler.h>
#include <iostream>
#include <algorithm>
#include <float.h>

#pragma comment(lib, "d3dcompiler.lib")

PropGameComponent::PropGameComponent(Game* game, const std::string& path, const Vector3& startPosition, float scale)
    : GameComponent(game), model(game), position(startPosition), modelPath(path), modelScale(scale)
    , collisionRadius(0.5f), collisionCenterOffset(0, 0, 0)
    , debugVertexBuffer(nullptr), debugIndexBuffer(nullptr), debugVS(nullptr), debugPS(nullptr)
    , debugLayout(nullptr), debugConstantBuffer(nullptr), debugInitialized(false) {
}

PropGameComponent::~PropGameComponent() {
    DestroyResources();
}

void PropGameComponent::UpdateCollisionData() {
    DirectX::BoundingBox localBox = model.GetLocalBoundingBox();
    Vector3 scale = model.GetScale();

    if (localBox.Extents.x == 0.5f && localBox.Extents.y == 0.5f && localBox.Extents.z == 0.5f &&
        localBox.Center.x == 0 && localBox.Center.y == 0 && localBox.Center.z == 0) {
        model.CalculateLocalBoundingBox();
        localBox = model.GetLocalBoundingBox();
    }

    Vector3 worldExtents = localBox.Extents * scale;

    collisionRadius = std::max({ worldExtents.x, worldExtents.y, worldExtents.z });

    collisionCenterOffset = localBox.Center * scale;

    if (collisionRadius < 0.1f) collisionRadius = 0.5f;

    std::cout << "[Prop] Collision - LocalBox Center: (" << localBox.Center.x << ", " << localBox.Center.y << ", " << localBox.Center.z << ")"
        << ", Extents: (" << localBox.Extents.x << ", " << localBox.Extents.y << ", " << localBox.Extents.z << ")"
        << ", Scale: (" << scale.x << ", " << scale.y << ", " << scale.z << ")"
        << ", WorldOffset: (" << collisionCenterOffset.x << ", " << collisionCenterOffset.y << ", " << collisionCenterOffset.z << ")"
        << ", Radius: " << collisionRadius << std::endl;
}

void PropGameComponent::InitDebugCollider() {
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

void PropGameComponent::DrawDebugCollider() {
    if (!debugInitialized || !game || !game->Context || !game->Camera) return;

    Vector3 worldCenter = GetCollisionCenter();
    float worldRadius = collisionRadius;

    struct DebugVertex { Vector3 position; Vector4 color; };
    std::vector<DebugVertex> vertices;
    std::vector<UINT> indices;

    int segments = 24;
    float angleStep = 2.0f * 3.14159f / segments;

    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float x = cos(angle) * worldRadius;
        float z = sin(angle) * worldRadius;
        vertices.push_back({ Vector3(worldCenter.x + x, worldCenter.y, worldCenter.z + z), Vector4(1, 0, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    int xzStart = (int)vertices.size() - (segments + 1);

    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float x = cos(angle) * worldRadius;
        float y = sin(angle) * worldRadius;
        vertices.push_back({ Vector3(worldCenter.x + x, worldCenter.y + y, worldCenter.z), Vector4(1, 0, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(xzStart + segments + 1 + i);
        indices.push_back(xzStart + segments + 1 + i + 1);
    }

    int xyStart = xzStart + segments + 1;

    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float y = cos(angle) * worldRadius;
        float z = sin(angle) * worldRadius;
        vertices.push_back({ Vector3(worldCenter.x, worldCenter.y + y, worldCenter.z + z), Vector4(1, 0, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(xyStart + segments + 1 + i);
        indices.push_back(xyStart + segments + 1 + i + 1);
    }

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

void PropGameComponent::Initialize() {
    if (model.Load(modelPath)) {
        model.SetPosition(position);
        model.SetScale(Vector3(modelScale, modelScale, modelScale));

        UpdateCollisionData();
        InitDebugCollider();

        std::cout << "[Prop] Loaded at (" << position.x << ", " << position.z
            << ") Scale: " << modelScale << ", CollisionRadius: " << collisionRadius << std::endl;
    }
    else {
        std::cout << "[Prop] ERROR: Failed to load " << modelPath << std::endl;
    }
}

void PropGameComponent::Update(float deltaTime) {
    // Static objects
}

void PropGameComponent::Draw() {
    if (!game || !game->Camera) return;
    if (!model.IsValid()) return;
    model.Draw();
    DrawDebugCollider();
}

void PropGameComponent::DestroyResources() {
    model.Unload();

    if (debugVertexBuffer) { debugVertexBuffer->Release(); debugVertexBuffer = nullptr; }
    if (debugIndexBuffer) { debugIndexBuffer->Release(); debugIndexBuffer = nullptr; }
    if (debugVS) { debugVS->Release(); debugVS = nullptr; }
    if (debugPS) { debugPS->Release(); debugPS = nullptr; }
    if (debugLayout) { debugLayout->Release(); debugLayout = nullptr; }
    if (debugConstantBuffer) { debugConstantBuffer->Release(); debugConstantBuffer = nullptr; }

    debugInitialized = false;
}

void PropGameComponent::SetPosition(const Vector3& pos) {
    position = pos;
    model.SetPosition(pos);
}