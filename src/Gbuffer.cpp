// GBuffer.cpp
#include "GBuffer.h"
#include <iostream>

GBuffer::GBuffer()
    : width(0)
    , height(0)
    , initialized(false)
    , depthTexture(nullptr)
    , depthDSV(nullptr)
    , depthSRV(nullptr) {
    for (int i = 0; i < NUM_TEXTURES; i++) {
        textures[i] = nullptr;
        rtvs[i] = nullptr;
        srvs[i] = nullptr;
    }
}

GBuffer::~GBuffer() {
    Release();
}

HRESULT GBuffer::Initialize(ID3D11Device* device, int w, int h) {
    if (initialized) return S_OK;
    if (!device) return E_POINTER;
    if (w <= 0 || h <= 0) return E_INVALIDARG;

    width = w;
    height = h;

    // ‘орматы дл€ G-Buffer текстур
    DXGI_FORMAT formats[NUM_TEXTURES] = {
        DXGI_FORMAT_R8G8B8A8_UNORM,      // DIFFUSE: 8 бит на канал, нормализованный
        DXGI_FORMAT_R16G16B16A16_FLOAT,  // NORMAL: 16 бит float, высока€ точность
        DXGI_FORMAT_R16G16B16A16_FLOAT,  // WORLD_POS: 16 бит float, высока€ точность
        DXGI_FORMAT_R8G8B8A8_UNORM       // SPECULAR: specular RGB + shininess в A
    };

    // —оздаем текстуры, RTV и SRV
    for (int i = 0; i < NUM_TEXTURES; i++) {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = formats[i];
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &textures[i]);
        if (FAILED(hr)) {
            Release();
            return hr;
        }

        hr = device->CreateRenderTargetView(textures[i], nullptr, &rtvs[i]);
        if (FAILED(hr)) {
            Release();
            return hr;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = formats[i];
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(textures[i], &srvDesc, &srvs[i]);
        if (FAILED(hr)) {
            Release();
            return hr;
        }
    }

    // —оздаем Depth текстуру
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&depthDesc, nullptr, &depthTexture);
    if (FAILED(hr)) {
        Release();
        return hr;
    }

    // Depth Stencil View
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    hr = device->CreateDepthStencilView(depthTexture, &dsvDesc, &depthDSV);
    if (FAILED(hr)) {
        Release();
        return hr;
    }

    // Depth Shader Resource View (дл€ отладки)
    D3D11_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
    depthSrvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    depthSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MostDetailedMip = 0;
    depthSrvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(depthTexture, &depthSrvDesc, &depthSRV);
    if (FAILED(hr)) {
        Release();
        return hr;
    }

    initialized = true;
    return S_OK;
}

void GBuffer::Release() {
    // ќсвобождаем текстуры, RTV и SRV
    for (int i = 0; i < NUM_TEXTURES; i++) {
        if (rtvs[i]) {
            rtvs[i]->Release();
            rtvs[i] = nullptr;
        }
        if (srvs[i]) {
            srvs[i]->Release();
            srvs[i] = nullptr;
        }
        if (textures[i]) {
            textures[i]->Release();
            textures[i] = nullptr;
        }
    }

    if (depthDSV) {
        depthDSV->Release();
        depthDSV = nullptr;
    }
    if (depthSRV) {
        depthSRV->Release();
        depthSRV = nullptr;
    }
    if (depthTexture) {
        depthTexture->Release();
        depthTexture = nullptr;
    }

    initialized = false;
    width = 0;
    height = 0;
}

void GBuffer::SetRenderTargets(ID3D11DeviceContext* context) {
    if (!initialized || !context) return;
    context->OMSetRenderTargets(NUM_TEXTURES, rtvs, depthDSV);
}

void GBuffer::Clear(ID3D11DeviceContext* context) {
    if (!initialized || !context) return;

    const float clearColor0[] = { 0.0f, 0.0f, 0.0f, 0.0f }; // DIFFUSE
    const float clearColor1[] = { 0.0f, 0.0f, 0.0f, 0.0f }; // NORMAL
    const float clearColor2[] = { 0.0f, 0.0f, 0.0f, 0.0f }; // WORLD_POS
    const float clearColor3[] = { 0.0f, 0.0f, 0.0f, 0.0f }; // SPECULAR

    context->ClearRenderTargetView(rtvs[DIFFUSE], clearColor0);
    context->ClearRenderTargetView(rtvs[NORMAL], clearColor1);
    context->ClearRenderTargetView(rtvs[WORLD_POS], clearColor2);
    context->ClearRenderTargetView(rtvs[SPECULAR], clearColor3);
    context->ClearDepthStencilView(depthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

ID3D11ShaderResourceView* GBuffer::GetSRV(TextureType type) const {
    if (type >= 0 && type < NUM_TEXTURES) {
        return srvs[type];
    }
    return nullptr;
}

void GBuffer::TestClearColors(ID3D11DeviceContext* context) {
    if (!initialized || !context) return;

    // ќчищаем каждый RTV своим €рким цветом
    float clearRed[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    float clearGreen[] = { 0.0f, 1.0f, 0.0f, 1.0f };
    float clearBlue[] = { 0.0f, 0.0f, 1.0f, 1.0f };
    float clearYellow[] = { 1.0f, 1.0f, 0.0f, 1.0f };

    context->ClearRenderTargetView(rtvs[0], clearRed);
    context->ClearRenderTargetView(rtvs[1], clearGreen);
    context->ClearRenderTargetView(rtvs[2], clearBlue);
    context->ClearRenderTargetView(rtvs[3], clearYellow);
    context->ClearDepthStencilView(depthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    std::cout << "GBuffer cleared with colors: RTV0=RED, RTV1=GREEN, RTV2=BLUE, RTV3=YELLOW" << std::endl;
}