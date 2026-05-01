#pragma once
#include <d3d11.h>
#include <SimpleMath.h>
#include <vector>
#include <iostream>

using namespace DirectX::SimpleMath;

class CascadeShadowMap {
public:
    static constexpr UINT CASCADE_COUNT = 4;
    static constexpr UINT SHADOW_MAP_SIZE = 2048;

private:
    struct CascadeData {
        float splitDepth;
        Matrix viewMatrix;
        Matrix projMatrix;
        Matrix viewProjMatrix;
        float nearPlane;
        float farPlane;
    };

    ID3D11Texture2D* shadowMapTexture = nullptr;
    ID3D11DepthStencilView* shadowMapDSVs[CASCADE_COUNT] = {};
    ID3D11ShaderResourceView* shadowMapSRVs[CASCADE_COUNT] = {};
    ID3D11SamplerState* shadowSampler = nullptr;
    ID3D11Buffer* cascadeConstantBuffer = nullptr;

    CascadeData cascades[CASCADE_COUNT];
    float cascadeSplitLambda = 0.95f;
    Vector3 lightDirection = Vector3(0.5f, -1.0f, 0.3f);
    bool initialized = false;

public:
    struct CascadeConstantBuffer {
        Matrix lightViewProj[CASCADE_COUNT];
        Vector4 cascadeSplits;
    };

    CascadeShadowMap() {
        lightDirection.Normalize();
    }

    ~CascadeShadowMap() {
        DestroyResources();
    }

    void DestroyResources() {
        if (shadowMapTexture) { shadowMapTexture->Release(); shadowMapTexture = nullptr; }
        for (int i = 0; i < CASCADE_COUNT; ++i) {
            if (shadowMapDSVs[i]) { shadowMapDSVs[i]->Release(); shadowMapDSVs[i] = nullptr; }
            if (shadowMapSRVs[i]) { shadowMapSRVs[i]->Release(); shadowMapSRVs[i] = nullptr; }
        }
        if (shadowSampler) { shadowSampler->Release(); shadowSampler = nullptr; }
        if (cascadeConstantBuffer) { cascadeConstantBuffer->Release(); cascadeConstantBuffer = nullptr; }
        initialized = false;
    }

    HRESULT Initialize(ID3D11Device* device) {
        if (!device) return E_FAIL;
        if (initialized) return S_OK;

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = SHADOW_MAP_SIZE;
        texDesc.Height = SHADOW_MAP_SIZE;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = CASCADE_COUNT;
        texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &shadowMapTexture);
        if (FAILED(hr)) {
            std::cout << "ERROR: Failed to create shadow map texture! HR=" << std::hex << hr << std::endl;
            return hr;
        }
        std::cout << "Shadow map texture created successfully" << std::endl;

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;  // ÈÇÌÅÍÅÍÎ
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;  // ÈÇÌÅÍÅÍÎ
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels = 1;

        for (UINT i = 0; i < CASCADE_COUNT; ++i) {
            dsvDesc.Texture2DArray.FirstArraySlice = i;
            dsvDesc.Texture2DArray.ArraySize = 1;
            hr = device->CreateDepthStencilView(shadowMapTexture, &dsvDesc, &shadowMapDSVs[i]);
            if (FAILED(hr)) return hr;

            srvDesc.Texture2DArray.FirstArraySlice = i;
            srvDesc.Texture2DArray.ArraySize = 1;
            hr = device->CreateShaderResourceView(shadowMapTexture, &srvDesc, &shadowMapSRVs[i]);
            if (FAILED(hr)) return hr;
        }

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.BorderColor[0] = 1.0f;
        samplerDesc.BorderColor[1] = 1.0f;
        samplerDesc.BorderColor[2] = 1.0f;
        samplerDesc.BorderColor[3] = 1.0f;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = device->CreateSamplerState(&samplerDesc, &shadowSampler);
        if (FAILED(hr)) return hr;

        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.ByteWidth = sizeof(CascadeConstantBuffer);
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        hr = device->CreateBuffer(&cbDesc, nullptr, &cascadeConstantBuffer);
        if (FAILED(hr)) return hr;

        initialized = true;
        return S_OK;
    }

    void UpdateCascades(const Matrix& cameraView, const Matrix& cameraProj, float cameraNear, float cameraFar) {
        float splits[CASCADE_COUNT];
        for (UINT i = 0; i < CASCADE_COUNT; ++i) {
            float p = (float)(i + 1) / CASCADE_COUNT;
            float logSplit = cameraNear * pow(cameraFar / cameraNear, p);
            float uniformSplit = cameraNear + (cameraFar - cameraNear) * p;
            splits[i] = cascadeSplitLambda * logSplit + (1.0f - cascadeSplitLambda) * uniformSplit;
        }

        Matrix invViewProj = (cameraView * cameraProj).Invert();

        for (UINT cascade = 0; cascade < CASCADE_COUNT; ++cascade) {
            float nearPlane = (cascade == 0) ? cameraNear : splits[cascade - 1];
            float farPlane = splits[cascade];

            cascades[cascade].nearPlane = nearPlane;
            cascades[cascade].farPlane = farPlane;
            cascades[cascade].splitDepth = farPlane;

            Vector3 frustumCorners[8];
            GetFrustumCorners(invViewProj, nearPlane, farPlane, frustumCorners);

            Vector3 frustumCenter = Vector3::Zero;
            for (int i = 0; i < 8; ++i) {
                frustumCenter += frustumCorners[i];
            }
            frustumCenter /= 8.0f;

            float radius = 0.0f;
            for (int i = 0; i < 8; ++i) {
                float dist = (frustumCorners[i] - frustumCenter).Length();
                radius = std::max(radius, dist);
            }

            float texelsPerUnit = SHADOW_MAP_SIZE / (radius * 2.0f);

            Vector3 lightPos = frustumCenter - lightDirection * radius;
            Vector3 up = Vector3(0, 1, 0);
            if (abs(lightDirection.Dot(up)) > 0.999f) {
                up = Vector3(1, 0, 0);
            }

            Matrix lightView = Matrix::CreateLookAt(lightPos, frustumCenter, up);
            Vector3 lightSpaceCenter = Vector3::Transform(frustumCenter, lightView);

            lightSpaceCenter.x = floor(lightSpaceCenter.x * texelsPerUnit) / texelsPerUnit;
            lightSpaceCenter.y = floor(lightSpaceCenter.y * texelsPerUnit) / texelsPerUnit;

            Matrix invLightView = lightView.Invert();
            Vector3 roundedWorldCenter = Vector3::Transform(lightSpaceCenter, invLightView);
            lightPos += (frustumCenter - roundedWorldCenter);
            lightView = Matrix::CreateLookAt(lightPos, frustumCenter, up);

            cascades[cascade].viewMatrix = lightView;

            Matrix lightProj = Matrix::CreateOrthographicOffCenter(
                -radius, radius, -radius, radius, 0.0f, radius * 2.0f);

            cascades[cascade].projMatrix = lightProj;
            cascades[cascade].viewProjMatrix = lightView * lightProj;
        }
    }

    void GetFrustumCorners(const Matrix& invViewProj, float nearPlane, float farPlane, Vector3* corners) {
        Vector4 ndcCorners[8] = {
            Vector4(-1, -1, 0, 1), Vector4(1, -1, 0, 1),
            Vector4(1,  1, 0, 1), Vector4(-1,  1, 0, 1),
            Vector4(-1, -1, 1, 1), Vector4(1, -1, 1, 1),
            Vector4(1,  1, 1, 1), Vector4(-1,  1, 1, 1)
        };

        for (int i = 0; i < 8; ++i) {
            Vector4 worldPos = Vector4::Transform(ndcCorners[i], invViewProj);
            corners[i] = Vector3(worldPos.x, worldPos.y, worldPos.z) / worldPos.w;
        }

        Vector3 nearCorners[4], farCorners[4];
        for (int i = 0; i < 4; ++i) {
            nearCorners[i] = corners[i];
            farCorners[i] = corners[i + 4];
        }

        for (int i = 0; i < 4; ++i) {
            corners[i] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * ((nearPlane - 0.0f) / (1.0f - 0.0f));
            corners[i + 4] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * ((farPlane - 0.0f) / (1.0f - 0.0f));
        }
    }

    void SetForShadowPass(ID3D11DeviceContext* context, UINT cascade) {
        if (!initialized || cascade >= CASCADE_COUNT) return;

        // Î×ÈÙÀÅÌ ÄÐÓÃÈÌ ÇÍÀ×ÅÍÈÅÌ ÄËß ÒÅÑÒÀ
        context->ClearDepthStencilView(shadowMapDSVs[cascade], D3D11_CLEAR_DEPTH, 0.5f, 0);  // ÈÇÌÅÍÈËÈ Ñ 1.0f ÍÀ 0.5f

        ID3D11RenderTargetView* nullRTV[1] = { nullptr };
        context->OMSetRenderTargets(0, nullRTV, shadowMapDSVs[cascade]);

        D3D11_VIEWPORT viewport = {};
        viewport.Width = (float)SHADOW_MAP_SIZE;
        viewport.Height = (float)SHADOW_MAP_SIZE;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;
        context->RSSetViewports(1, &viewport);

        std::cout << "Cleared cascade " << cascade << " to 0.5" << std::endl;
    }

    void UpdateConstantBuffer(ID3D11DeviceContext* context) {
        if (!initialized) return;

        CascadeConstantBuffer cb;
        for (int i = 0; i < CASCADE_COUNT; ++i) {
            cb.lightViewProj[i] = cascades[i].viewProjMatrix.Transpose();
        }
        cb.cascadeSplits = Vector4(cascades[0].splitDepth, cascades[1].splitDepth,
            cascades[2].splitDepth, cascades[3].splitDepth);
        context->UpdateSubresource(cascadeConstantBuffer, 0, nullptr, &cb, 0, 0);
    }

    Matrix GetLightViewMatrix(UINT cascade) const { return cascades[cascade].viewMatrix; }
    Matrix GetLightProjMatrix(UINT cascade) const { return cascades[cascade].projMatrix; }
    Matrix GetLightViewProjMatrix(UINT cascade) const { return cascades[cascade].viewProjMatrix; }
    ID3D11ShaderResourceView* GetSRV(UINT cascade) const { return shadowMapSRVs[cascade]; }
    ID3D11SamplerState* GetSampler() const { return shadowSampler; }
    ID3D11Buffer* GetConstantBuffer() const { return cascadeConstantBuffer; }

    void SetLightDirection(const Vector3& dir) {
        lightDirection = dir;
        lightDirection.Normalize();
    }

    bool IsInitialized() const { return initialized; }

    float GetSplitDepth(UINT cascade) const {
        return (cascade < CASCADE_COUNT) ? cascades[cascade].splitDepth : 0.0f;
    }
};