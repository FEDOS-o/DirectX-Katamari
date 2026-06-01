#include "CascadeShadowMap.h"
#include <algorithm>
#include <cmath>
#include <iostream>

CascadeShadowMap::CascadeShadowMap(Game* inGame, int mapSize)
    : game(inGame), shadowMapSize(mapSize) {
}

CascadeShadowMap::~CascadeShadowMap() {
    DestroyResources();
}

std::vector<float> CascadeShadowMap::CalculateSplitDistances(float nearPlane, float farPlane, float lambda) {
    std::vector<float> splits(CASCADE_COUNT + 1);
    splits[0] = nearPlane;

    for (int i = 1; i <= CASCADE_COUNT; ++i) {
        float ratio = static_cast<float>(i) / CASCADE_COUNT;
        float logSplit = nearPlane * std::pow(farPlane / nearPlane, ratio);
        float linearSplit = nearPlane + (farPlane - nearPlane) * ratio;
        splits[i] = lambda * logSplit + (1.0f - lambda) * linearSplit;
    }

    return splits;
}

Matrix CascadeShadowMap::ComputeCascadeLightMatrix(const Matrix& cameraView, const Matrix& cameraProj,
    float splitNear, float splitFar,
    const Vector3& lightDir) {
    Matrix invViewProj = (cameraView * cameraProj).Invert();

    Vector3 ndcCorners[8] = {
        Vector3(-1,  1, 0), Vector3(1,  1, 0), Vector3(1, -1, 0), Vector3(-1, -1, 0),
        Vector3(-1,  1, 1), Vector3(1,  1, 1), Vector3(1, -1, 1), Vector3(-1, -1, 1)
    };

    Vector3 worldCorners[8];

    for (int i = 0; i < 8; ++i) {
        float z = (i < 4) ? splitNear : splitFar;
        // Перспективное деление для корректного Z
        Vector4 clipPos = Vector4(ndcCorners[i].x, ndcCorners[i].y,
            (z - 0.1f) / (1000.0f - 0.1f) * 2.0f - 1.0f, 1.0f);
        Vector4 worldPos = Vector4::Transform(clipPos, invViewProj);
        if (worldPos.w != 0) worldPos /= worldPos.w;
        worldCorners[i] = Vector3(worldPos.x, worldPos.y, worldPos.z);
    }

    Vector3 center = Vector3::Zero;
    for (int i = 0; i < 8; ++i) center += worldCorners[i];
    center /= 8.0f;

    float radius = 0.0f;
    for (int i = 0; i < 8; ++i) {
        radius = std::max(radius, (worldCorners[i] - center).Length());
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    Vector3 lightPos = center - lightDir * (radius * 2.0f);
    Vector3 up = (std::abs(lightDir.y) > 0.99f) ? Vector3(0, 0, 1) : Vector3(0, 1, 0);

    Matrix lightView = Matrix::CreateLookAt(lightPos, center, up);
    float orthoSize = radius;
    Matrix lightProj = Matrix::CreateOrthographic(orthoSize * 2, orthoSize * 2, -radius * 4, radius * 4);

    // Snap к texel grid
    Matrix lightViewProj = lightView * lightProj;
    Vector4 origin = Vector4::Transform(Vector4(0, 0, 0, 1), lightViewProj);
    origin /= origin.w;

    float texelSize = (orthoSize * 2) / shadowMapSize;
    Vector2 offset(
        std::floor(origin.x / texelSize) * texelSize - origin.x,
        std::floor(origin.y / texelSize) * texelSize - origin.y
    );

    Matrix snapMatrix = Matrix::CreateTranslation(Vector3(offset.x, offset.y, 0));
    lightProj = lightProj * snapMatrix;

    return lightView * lightProj;
}

bool CascadeShadowMap::Initialize() {
    if (!game || !game->Device) return false;

    for (int i = 0; i < CASCADE_COUNT; ++i) {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = shadowMapSize;
        texDesc.Height = shadowMapSize;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = game->Device->CreateTexture2D(&texDesc, nullptr, &cascades[i].texture);
        if (FAILED(hr)) {
            std::cout << "Failed to create CSM texture " << i << std::endl;
            DestroyResources();
            return false;
        }

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;

        hr = game->Device->CreateDepthStencilView(cascades[i].texture, &dsvDesc, &cascades[i].dsv);
        if (FAILED(hr)) {
            DestroyResources();
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = game->Device->CreateShaderResourceView(cascades[i].texture, &srvDesc, &cascades[i].srv);
        if (FAILED(hr)) {
            DestroyResources();
            return false;
        }
    }

    D3D11_SAMPLER_DESC compSamplerDesc = {};
    compSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    compSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    compSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    compSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    compSamplerDesc.BorderColor[0] = 1.0f;
    compSamplerDesc.BorderColor[1] = 1.0f;
    compSamplerDesc.BorderColor[2] = 1.0f;
    compSamplerDesc.BorderColor[3] = 1.0f;
    compSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
    compSamplerDesc.MinLOD = 0;
    compSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    game->Device->CreateSamplerState(&compSamplerDesc, &comparisonSampler);

    D3D11_SAMPLER_DESC pointSamplerDesc = {};
    pointSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    pointSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    pointSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    pointSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    pointSamplerDesc.BorderColor[0] = 1.0f;
    pointSamplerDesc.BorderColor[1] = 1.0f;
    pointSamplerDesc.BorderColor[2] = 1.0f;
    pointSamplerDesc.BorderColor[3] = 1.0f;

    game->Device->CreateSamplerState(&pointSamplerDesc, &pointSampler);

    initialized = true;
    std::cout << "CascadeShadowMap initialized with " << CASCADE_COUNT << " cascades at "
        << shadowMapSize << "x" << shadowMapSize << std::endl;
    return true;
}

void CascadeShadowMap::DestroyResources() {
    for (int i = 0; i < CASCADE_COUNT; ++i) {
        if (cascades[i].dsv) { cascades[i].dsv->Release(); cascades[i].dsv = nullptr; }
        if (cascades[i].srv) { cascades[i].srv->Release(); cascades[i].srv = nullptr; }
        if (cascades[i].texture) { cascades[i].texture->Release(); cascades[i].texture = nullptr; }
    }

    if (comparisonSampler) { comparisonSampler->Release(); comparisonSampler = nullptr; }
    if (pointSampler) { pointSampler->Release(); pointSampler = nullptr; }

    initialized = false;
}

void CascadeShadowMap::UpdateCascades(const Matrix& cameraView, const Matrix& cameraProj,
    float nearPlane, float farPlane, const Vector3& lightDir) {
    if (!initialized) return;

    auto splits = CalculateSplitDistances(nearPlane, farPlane, 0.6f);

    for (int i = 0; i < CASCADE_COUNT; ++i) {
        cascades[i].splitNear = splits[i];
        cascades[i].splitFar = splits[i + 1];
        cascades[i].lightViewProj = ComputeCascadeLightMatrix(cameraView, cameraProj,
            splits[i], splits[i + 1], lightDir);
    }
}

void CascadeShadowMap::BeginCascadeRender(int cascadeIndex) {
    if (cascadeIndex < 0 || cascadeIndex >= CASCADE_COUNT) return;

    auto& cascade = cascades[cascadeIndex];

    game->Context->ClearDepthStencilView(cascade.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

    ID3D11RenderTargetView* nullRTV = nullptr;
    game->Context->OMSetRenderTargets(0, &nullRTV, cascade.dsv);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(shadowMapSize);
    viewport.Height = static_cast<float>(shadowMapSize);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    game->Context->RSSetViewports(1, &viewport);

    ShadowConstantBufferCombined shadowCB;
    shadowCB.lightViewProj = cascade.lightViewProj.Transpose();
    game->Context->UpdateSubresource(game->shadowConstantBuffer, 0, nullptr, &shadowCB, 0, 0);
    game->Context->VSSetConstantBuffers(0, 1, &game->shadowConstantBuffer);
}

void CascadeShadowMap::EndCascadeRender() {
    // Восстановление происходит в Game::Draw()
}

Vector4 CascadeShadowMap::GetCascadeSplits() const {
    return Vector4(cascades[0].splitFar, cascades[1].splitFar,
        cascades[2].splitFar, cascades[3].splitFar);
}