#include "Prop.h"
#include "Game.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <float.h>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

Prop::Prop(Game* game, const std::string& path, const Vector3& startPosition, float scale)
    : GameComponent(game), model(game), position(startPosition), modelPath(path), modelScale(scale) {
}

Prop::~Prop() { DestroyResources(); }

ID3DBlob* Prop::CompileDebugShader(const char* code, const char* target, const char* entryPoint) {
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

void Prop::UpdateCollisionData() {
    DirectX::BoundingBox localBox = model.GetLocalBoundingBox();
    Vector3 scale = model.GetScale();

    if (localBox.Extents.x == 0.5f && localBox.Center.x == 0) {
        model.CalculateLocalBoundingBox();
        localBox = model.GetLocalBoundingBox();
    }

    Vector3 worldExtents = localBox.Extents * scale;
    collisionRadius = std::max({ worldExtents.x, worldExtents.y, worldExtents.z });
    collisionCenterOffset = localBox.Center * scale;
    if (collisionRadius < 0.1f) collisionRadius = 0.5f;
}

void Prop::InitDebugCollider() {
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

void Prop::DrawDebugCollider() {
    if (!debugInitialized || !game || !game->Context || !game->Camera) return;

    Vector3 worldCenter = GetCollisionCenter();
    float worldRadius = collisionRadius;

    struct DebugVertex { Vector3 position; Vector4 color; };
    std::vector<DebugVertex> vertices;
    std::vector<UINT> indices;

    constexpr int SEGMENTS = 24;
    constexpr float ANGLE_STEP = XM_2PI / SEGMENTS;

    // Three circles
    for (int c = 0; c < 3; c++) {
        int startIdx = (int)vertices.size();
        for (int i = 0; i <= SEGMENTS; i++) {
            float angle = i * ANGLE_STEP;
            Vector3 pos;
            if (c == 0) pos = Vector3(worldCenter.x + cos(angle) * worldRadius, worldCenter.y, worldCenter.z + sin(angle) * worldRadius);
            else if (c == 1) pos = Vector3(worldCenter.x + cos(angle) * worldRadius, worldCenter.y + sin(angle) * worldRadius, worldCenter.z);
            else pos = Vector3(worldCenter.x, worldCenter.y + cos(angle) * worldRadius, worldCenter.z + sin(angle) * worldRadius);

            vertices.push_back({ pos, Vector4(1, 0, 0, 1) });
            if (i < SEGMENTS) { indices.push_back(startIdx + i); indices.push_back(startIdx + i + 1); }
        }
    }

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

void Prop::Initialize() {
    if (model.Load(modelPath)) {
        model.SetPosition(position);
        model.SetScale(Vector3(modelScale, modelScale, modelScale));
        UpdateCollisionData();
        InitDebugCollider();
    }
}

void Prop::Update(float deltaTime) {}

void Prop::Draw() {
    if (!game || !game->Camera || !model.IsValid()) return;
    model.Draw();
    DrawDebugCollider();
}

void Prop::DestroyResources() {
    model.Unload();

    if (debugVertexBuffer) { debugVertexBuffer->Release(); debugVertexBuffer = nullptr; }
    if (debugIndexBuffer) { debugIndexBuffer->Release(); debugIndexBuffer = nullptr; }
    if (debugVS) { debugVS->Release(); debugVS = nullptr; }
    if (debugPS) { debugPS->Release(); debugPS = nullptr; }
    if (debugLayout) { debugLayout->Release(); debugLayout = nullptr; }
    if (debugConstantBuffer) { debugConstantBuffer->Release(); debugConstantBuffer = nullptr; }

    debugInitialized = false;
}

void Prop::SetPosition(const Vector3& pos) {
    position = pos;
    model.SetPosition(pos);
}