#include "Game.h"
#include "Core.h"
#include "OrbitalCamera.h"
#include "FirstPersonCamera.h"
#include "DisplayWin32.h"
#include "InputDevice.h"
#include "ShadowRenderer.h"
#include <iostream>
#include <algorithm>

Game::Game(LPCWSTR applicationName, HINSTANCE hInstance, LONG screenWidth, LONG screenHeight) :
    Instance(hInstance),
    Name(applicationName),
    TotalTime(0.0f),
    ScreenResized(false),
    Display(nullptr),
    Input(nullptr),
    isUsingOrbitalCamera(true),
    firstPersonCamera(nullptr),
    orbitalCamera(nullptr),
    DepthStencilBuffer(nullptr),
    DepthStencilView(nullptr),
    DepthStencilState(nullptr)
{
    Display = new DisplayWin32(this, screenWidth, screenHeight, hInstance, applicationName);
    Input = new InputDevice(this);

    D3D_FEATURE_LEVEL featureLevel[] = { D3D_FEATURE_LEVEL_11_1 };

    DXGI_SWAP_CHAIN_DESC swapDesc = {};
    swapDesc.BufferCount = 2;
    swapDesc.BufferDesc.Width = screenWidth;
    swapDesc.BufferDesc.Height = screenHeight;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.OutputWindow = Display->Window;
    swapDesc.Windowed = true;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;

    auto res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_DEBUG,
        featureLevel,
        1,
        D3D11_SDK_VERSION,
        &swapDesc,
        &SwapChain,
        &Device,
        nullptr,
        &Context);

    if (FAILED(res))
    {
        Display->createMessageBox(L"Failed to create D3D11 device and swap chain", L"Error", MB_OK);
    }

    Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&DebugAnnotation);

    orbitalCamera = new OrbitalCamera(this, Vector3(0, 2, 0), 25.0f);
    firstPersonCamera = new FirstPersonCamera(this, Vector3(0, 5, 15));
    Camera = orbitalCamera;

    SunLight.direction = Vector3(0.5f, -1.0f, 0.3f);
    SunLight.direction.Normalize();
    SunLight.ambient = Vector4(0.3f, 0.3f, 0.3f, 1.0f);
    SunLight.diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    SunLight.specular = Vector4(1.0f, 1.0f, 1.0f, 1.0f);

    PrevTime = std::chrono::steady_clock::now();
    StartTime = PrevTime;
}

Game::~Game() {
    DestroyResources();
}

HRESULT Game::CreateBackBuffer() {
    if (RenderView) {
        RenderView->Release();
        RenderView = nullptr;
    }
    if (BackBuffer) {
        BackBuffer->Release();
        BackBuffer = nullptr;
    }

    auto res = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to get back buffer", L"Error", MB_OK);
        return res;
    }

    res = Device->CreateRenderTargetView(BackBuffer, nullptr, &RenderView);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to create render target view", L"Error", MB_OK);
        return res;
    }
    return S_OK;
}

HRESULT Game::CreateDepthBuffer() {
    if (DepthStencilView) {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }
    if (DepthStencilBuffer) {
        DepthStencilBuffer->Release();
        DepthStencilBuffer = nullptr;
    }

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = 800;
    depthDesc.Height = 800;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    HRESULT res = Device->CreateTexture2D(&depthDesc, nullptr, &DepthStencilBuffer);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to create depth stencil buffer", L"Error", MB_OK);
        return res;
    }

    res = Device->CreateDepthStencilView(DepthStencilBuffer, nullptr, &DepthStencilView);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to create depth stencil view", L"Error", MB_OK);
        return res;
    }

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dsDesc.StencilEnable = false;

    res = Device->CreateDepthStencilState(&dsDesc, &DepthStencilState);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to create depth stencil state", L"Error", MB_OK);
        return res;
    }

    return S_OK;
}

HRESULT Game::Initialize() {
    auto res = CreateBackBuffer();
    if (FAILED(res)) {
        return res;
    }

    res = CreateDepthBuffer();
    if (FAILED(res)) {
        return res;
    }

    res = CreateShadowMapResources();
    if (FAILED(res)) {
        return res;
    }

    // Создаём CSM ресурсы (пока не используются)
    res = CreateCSMResources();
    if (FAILED(res)) {
        return res;
    }

    CD3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.CullMode = D3D11_CULL_FRONT;
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.DepthClipEnable = true;

    res = Device->CreateRasterizerState(&rastDesc, &RasterizerState);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to create rasterizer state", L"Error", MB_OK);
        return res;
    }

    orbitalCamera->Initialize();
    firstPersonCamera->Initialize();

    ShadowRendererComp = new Render::ShadowRenderer();
    ShadowRendererComp->Initialize(this);

    for (auto* component : components) {
        component->Initialize();
    }

    return S_OK;
}

void Game::Update() {
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - PrevTime).count() / 1000000.0f;

    PrevTime = currentTime;
    TotalTime += deltaTime;

    static bool cWasPressed = false;
    if (Input && Input->IsKeyDown(Keys::C)) {
        if (!cWasPressed) {
            SwitchCamera();
            cWasPressed = true;
        }
    }
    else {
        cWasPressed = false;
    }

    Camera->Update(deltaTime);

    for (auto* component : components) {
        component->Update(deltaTime);
    }

    UpdateLight(deltaTime);
    UpdateInternal(deltaTime);
}

void Game::UpdateInternal(float deltaTime) {
}

void Game::PrepareFrame() {
    Context->ClearState();

    if (ScreenResized) {
        RestoreTargets();
        ScreenResized = false;
    }

    D3D11_VIEWPORT viewport{};
    viewport.Width = 800.0f;
    viewport.Height = 800.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    Context->RSSetViewports(1, &viewport);
}

void Game::PrepareResources() {
}

void Game::RestoreTargets() {
    if (RenderView) {
        RenderView->Release();
        RenderView = nullptr;
    }
    if (BackBuffer) {
        BackBuffer->Release();
        BackBuffer = nullptr;
    }
    if (DepthStencilView) {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }
    if (DepthStencilBuffer) {
        DepthStencilBuffer->Release();
        DepthStencilBuffer = nullptr;
    }

    CreateBackBuffer();
    CreateDepthBuffer();
}

void Game::Draw() {
    // Обновляем каскады
    UpdateCascades();

    // ====== ПРОХОД 1: CSM Shadow Pass (4 каскада) ======
    for (UINT cascade = 0; cascade < CASCADE_COUNT; ++cascade) {
        PrepareCSMShadowPass(cascade);

        if (ShadowRendererComp) {
            ShadowRendererComp->BeginShadowPass(this);
        }

        Context->VSSetConstantBuffers(0, 1, &shadowConstantBuffer);

        for (auto* component : components) {
            component->DrawShadow();
        }

        if (ShadowRendererComp) {
            ShadowRendererComp->EndShadowPass(this);
        }
    }

    // ====== Восстанавливаем render target ======
    Context->OMSetRenderTargets(1, &RenderView, DepthStencilView);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = 800.0f;
    viewport.Height = 800.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    Context->RSSetViewports(1, &viewport);

    Context->OMSetDepthStencilState(DepthStencilState, 1);
    Context->RSSetState(RasterizerState);

    // ====== ПРОХОД 2: Основной рендеринг с тенями ======
    float clearColor[] = { 0.05f, 0.05f, 0.1f, 1.0f };
    Context->ClearRenderTargetView(RenderView, clearColor);
    Context->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Основной рендеринг (шейдеры сами устанавливают shadow ресурсы)
    for (auto* component : components) {
        component->Draw();
    }
}

void Game::EndFrame() {
    SwapChain->Present(1, 0);
}

void Game::MessageHandler(MSG& msg) {
    switch (msg.message) {
    case WM_KEYDOWN:
        if (msg.wParam == VK_ESCAPE) {
            Exit();
        }
        break;
    case WM_SIZE:
        ScreenResized = true;
        break;
    }
}

void Game::Exit() {
    PostQuitMessage(0);
}

void Game::DestroyResources() {
    if (orbitalCamera) {
        orbitalCamera->DestroyResources();
        delete orbitalCamera;
        orbitalCamera = nullptr;
    }
    if (firstPersonCamera) {
        firstPersonCamera->DestroyResources();
        delete firstPersonCamera;
        firstPersonCamera = nullptr;
    }

    for (auto* component : components) {
        component->DestroyResources();
    }

    if (DepthStencilState) { DepthStencilState->Release(); DepthStencilState = nullptr; }
    if (DepthStencilView) { DepthStencilView->Release(); DepthStencilView = nullptr; }
    if (DepthStencilBuffer) { DepthStencilBuffer->Release(); DepthStencilBuffer = nullptr; }
    if (RasterizerState) { RasterizerState->Release(); RasterizerState = nullptr; }
    if (RenderView) { RenderView->Release(); RenderView = nullptr; }
    if (BackBuffer) { BackBuffer->Release(); BackBuffer = nullptr; }
    if (RenderSRV) { RenderSRV->Release(); RenderSRV = nullptr; }
    if (DebugAnnotation) { DebugAnnotation->Release(); DebugAnnotation = nullptr; }
    if (Context) { Context->Release(); Context = nullptr; }
    if (SwapChain) { SwapChain->Release(); SwapChain = nullptr; }
    if (Display) { delete Display; Display = nullptr; }
    if (Input) { delete Input; Input = nullptr; }

    // Старые ресурсы
    if (ShadowMapTexture) { ShadowMapTexture->Release(); ShadowMapTexture = nullptr; }
    if (ShadowMapDSV) { ShadowMapDSV->Release(); ShadowMapDSV = nullptr; }
    if (ShadowMapSRV) { ShadowMapSRV->Release(); ShadowMapSRV = nullptr; }
    if (ShadowSampler) { ShadowSampler->Release(); ShadowSampler = nullptr; }
    if (shadowConstantBuffer) { shadowConstantBuffer->Release(); shadowConstantBuffer = nullptr; }
    if (ShadowVertexShader) { ShadowVertexShader->Release(); ShadowVertexShader = nullptr; }
    if (ShadowPixelShader) { ShadowPixelShader->Release(); ShadowPixelShader = nullptr; }
    if (ShadowInputLayout) { ShadowInputLayout->Release(); ShadowInputLayout = nullptr; }
    if (ShadowRendererComp) { delete ShadowRendererComp; ShadowRendererComp = nullptr; }
    if (shadowWorldConstantBuffer) { shadowWorldConstantBuffer->Release(); shadowWorldConstantBuffer = nullptr; }

    // Новые CSM ресурсы
    if (CSMShadowMapTexture) { CSMShadowMapTexture->Release(); CSMShadowMapTexture = nullptr; }
    for (int i = 0; i < CASCADE_COUNT; ++i) {
        if (CSMShadowMapDSVs[i]) { CSMShadowMapDSVs[i]->Release(); CSMShadowMapDSVs[i] = nullptr; }
        if (CSMShadowMapSRVs[i]) { CSMShadowMapSRVs[i]->Release(); CSMShadowMapSRVs[i] = nullptr; }
    }
    if (csmConstantBuffer) { csmConstantBuffer->Release(); csmConstantBuffer = nullptr; }
}

void Game::Run() {
    MSG msg = {};

    while (true) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            MessageHandler(msg);
        }

        if (msg.message == WM_QUIT) {
            break;
        }

        Update();
        PrepareFrame();
        PrepareResources();
        Draw();
        EndFrame();
    }
}

void Game::SwitchCamera() {
    isUsingOrbitalCamera = !isUsingOrbitalCamera;
    if (isUsingOrbitalCamera) {
        Vector3 fpsPos = firstPersonCamera->GetPosition();
        orbitalCamera->SetTarget(Vector3(fpsPos.x, 0, fpsPos.z));
        Camera = orbitalCamera;
    }
    else {
        Vector3 orbitalPos = orbitalCamera->GetPosition();
        firstPersonCamera->SetPosition(orbitalPos);
        Camera = firstPersonCamera;
    }
}

void Game::UpdateLight(float deltaTime) {
    // Анимируем свет для проверки теней
    static float lightAngle = 0.0f;
    lightAngle += deltaTime * 0.2f;

    SunLight.direction.x = sin(lightAngle) * 0.5f;
    SunLight.direction.y = -1.0f;
    SunLight.direction.z = cos(lightAngle) * 0.5f;
    SunLight.direction.Normalize();
}

// ===== СТАРЫЕ МЕТОДЫ (работают как раньше) =====

HRESULT Game::CreateShadowMapResources() {
    if (!Device) return E_FAIL;

    // Текстура для shadow map
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = SHADOW_MAP_SIZE;
    texDesc.Height = SHADOW_MAP_SIZE;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&texDesc, nullptr, &ShadowMapTexture);
    if (FAILED(hr)) return hr;

    // Depth Stencil View
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(ShadowMapTexture, &dsvDesc, &ShadowMapDSV);
    if (FAILED(hr)) return hr;

    // Shader Resource View
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = Device->CreateShaderResourceView(ShadowMapTexture, &srvDesc, &ShadowMapSRV);
    if (FAILED(hr)) return hr;

    // Сэмплер для сравнения теней
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

    hr = Device->CreateSamplerState(&samplerDesc, &ShadowSampler);
    if (FAILED(hr)) return hr;

    // Константные буферы
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    cbDesc.ByteWidth = sizeof(ShadowConstantBuffer);
    hr = Device->CreateBuffer(&cbDesc, nullptr, &shadowConstantBuffer);
    if (FAILED(hr)) return hr;

    cbDesc.ByteWidth = sizeof(ShadowWorldConstantBuffer);
    hr = Device->CreateBuffer(&cbDesc, nullptr, &shadowWorldConstantBuffer);

    ShadowBias = 0.00005f;

    return hr;
}

void Game::PrepareShadowPass() {
    if (!Context || !ShadowMapDSV) return;

    Context->ClearDepthStencilView(ShadowMapDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    ID3D11RenderTargetView* nullRTV[1] = { nullptr };
    Context->OMSetRenderTargets(0, nullRTV, ShadowMapDSV);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = (float)SHADOW_MAP_SIZE;
    viewport.Height = (float)SHADOW_MAP_SIZE;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    Context->RSSetViewports(1, &viewport);

    // Фиксированные параметры для стабильных теней
    Vector3 lightPos = Vector3(-20.0f, 30.0f, -20.0f);
    Vector3 lightTarget = Vector3(0.0f, 2.0f, 0.0f);
    Vector3 up = Vector3(0, 1, 0);

    // Используем направление света для позиции
    Vector3 lightDir = SunLight.direction;
    lightDir.Normalize();
    lightPos = lightTarget - lightDir * 50.0f;

    lightViewMatrix = Matrix::CreateLookAt(lightPos, lightTarget, up);

    // Ортографическая проекция
    float orthoSize = 40.0f;
    float nearPlane = 1.0f;
    float farPlane = 100.0f;
    lightProjectionMatrix = Matrix::CreateOrthographic(orthoSize, orthoSize, nearPlane, farPlane);

    // Комбинированная матрица
    ShadowConstantBuffer shadowCB;
    for (int i = 0; i < 4; i++) {
        shadowCB.lightView[i] = lightViewMatrix.Transpose();
        shadowCB.lightProjection[i] = lightProjectionMatrix.Transpose();
    }
    Context->UpdateSubresource(shadowConstantBuffer, 0, nullptr, &shadowCB, 0, 0);
    Context->VSSetConstantBuffers(0, 1, &shadowConstantBuffer);
}

void Game::SetShadowForRender() {
    // Этот метод больше не используется, всё делается в Draw()
}

HRESULT Game::CreateShadowShaders() {
    // Устаревший метод, шейдеры создаются в ShadowRenderer
    return S_OK;
}

void Game::SetShadowWorldMatrix(const Matrix& world) {
    if (!Context || !shadowWorldConstantBuffer) return;

    ShadowWorldConstantBuffer cb;
    cb.world = world.Transpose();
    Context->UpdateSubresource(shadowWorldConstantBuffer, 0, nullptr, &cb, 0, 0);
    Context->VSSetConstantBuffers(1, 1, &shadowWorldConstantBuffer);
}

Matrix Game::GetLightViewMatrix() const {
    return lightViewMatrix;
}

Matrix Game::GetLightProjectionMatrix() const {
    return lightProjectionMatrix;
}

// ===== НОВЫЕ CSM МЕТОДЫ (пока не используются) =====

HRESULT Game::CreateCSMResources() {
    if (!Device) return E_FAIL;

    // Текстурный массив для CSM
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = CSM_SHADOW_MAP_SIZE;
    texDesc.Height = CSM_SHADOW_MAP_SIZE;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = CASCADE_COUNT;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = Device->CreateTexture2D(&texDesc, nullptr, &CSMShadowMapTexture);
    if (FAILED(hr)) return hr;

    // DSV для каждого каскада (отдельный срез)
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.MipSlice = 0;

    for (UINT i = 0; i < CASCADE_COUNT; ++i) {
        dsvDesc.Texture2DArray.FirstArraySlice = i;
        dsvDesc.Texture2DArray.ArraySize = 1;
        hr = Device->CreateDepthStencilView(CSMShadowMapTexture, &dsvDesc, &CSMShadowMapDSVs[i]);
        if (FAILED(hr)) return hr;
    }

    // ОДИН SRV на ВЕСЬ Texture2DArray (все 4 среза)
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDescAll = {};
    srvDescAll.Format = DXGI_FORMAT_R32_FLOAT;
    srvDescAll.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDescAll.Texture2DArray.MostDetailedMip = 0;
    srvDescAll.Texture2DArray.MipLevels = 1;
    srvDescAll.Texture2DArray.FirstArraySlice = 0;
    srvDescAll.Texture2DArray.ArraySize = CASCADE_COUNT;  // ВСЕ 4 среза

    hr = Device->CreateShaderResourceView(CSMShadowMapTexture, &srvDescAll, &CSMShadowMapSRVs[0]);
    if (FAILED(hr)) return hr;

    // Остальные слоты SRV не нужны, но оставим для совместимости
    for (UINT i = 1; i < CASCADE_COUNT; ++i) {
        CSMShadowMapSRVs[i] = nullptr;
    }

    // Константный буфер для CSM
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(CSMConstantBuffer);
    hr = Device->CreateBuffer(&cbDesc, nullptr, &csmConstantBuffer);

    return hr;
}

void Game::UpdateCascades() {
    // Параметры основной камеры
    float camNear = 0.5f;
    float camFar = 200.0f;
    Matrix camView = Camera->GetViewMatrix();
    Matrix camProj = Camera->GetProjectionMatrix();

    // Вычисляем split distances (глубины разбиения каскадов)
    float splits[CASCADE_COUNT];
    for (UINT i = 0; i < CASCADE_COUNT; ++i) {
        float p = (float)(i + 1) / CASCADE_COUNT;
        float logSplit = camNear * pow(camFar / camNear, p);
        float uniformSplit = camNear + (camFar - camNear) * p;
        splits[i] = cascadeSplitLambda * logSplit + (1.0f - cascadeSplitLambda) * uniformSplit;
    }

    // Инвертированная ViewProj матрица камеры для извлечения frustum corners
    Matrix invCamViewProj = (camView * camProj).Invert();

    Vector3 lightDir = SunLight.direction;
    lightDir.Normalize();

    Vector3 up = Vector3(0, 1, 0);
    // Если lightDir параллелен up, используем другой up
    if (abs(lightDir.Dot(up)) > 0.999f) {
        up = Vector3(1, 0, 0);
    }

    for (UINT cascade = 0; cascade < CASCADE_COUNT; ++cascade) {
        float cascadeNear = (cascade == 0) ? camNear : splits[cascade - 1];
        float cascadeFar = splits[cascade];

        cascades[cascade].splitDepth = cascadeFar;

        // Получаем 8 углов frustum суб-секции в world space
        Vector3 frustumCorners[8];

        // NDC куба в порядке: near-bottom-left, near-bottom-right, near-top-right, near-top-left,
        //                      far-bottom-left,  far-bottom-right,  far-top-right,  far-top-left
        Vector4 ndcCorners[8] = {
            Vector4(-1, -1, 0, 1), // near-bottom-left
            Vector4(1, -1, 0, 1), // near-bottom-right
            Vector4(1,  1, 0, 1), // near-top-right
            Vector4(-1,  1, 0, 1), // near-top-left
            Vector4(-1, -1, 1, 1), // far-bottom-left
            Vector4(1, -1, 1, 1), // far-bottom-right
            Vector4(1,  1, 1, 1), // far-top-right
            Vector4(-1,  1, 1, 1)  // far-top-left
        };

        // Преобразуем NDC углы в world space
        for (int i = 0; i < 8; ++i) {
            Vector4 worldPos = Vector4::Transform(ndcCorners[i], invCamViewProj);
            frustumCorners[i] = Vector3(worldPos.x, worldPos.y, worldPos.z) / worldPos.w;
        }

        // Интерполируем углы для конкретного каскада
        Vector3 nearCorners[4], farCorners[4];
        for (int i = 0; i < 4; ++i) {
            nearCorners[i] = frustumCorners[i];
            farCorners[i] = frustumCorners[i + 4];
        }

        Vector3 cascadeCorners[8];
        for (int i = 0; i < 4; ++i) {
            // Линейная интерполяция между near и far плоскостями камеры
            float nearT = (cascadeNear - camNear) / (camFar - camNear);
            float farT = (cascadeFar - camNear) / (camFar - camNear);
            cascadeCorners[i] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * nearT;
            cascadeCorners[i + 4] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * farT;
        }

        // Центр frustum каскада
        Vector3 frustumCenter = Vector3::Zero;
        for (int i = 0; i < 8; ++i) {
            frustumCenter += cascadeCorners[i];
        }
        frustumCenter /= 8.0f;

        // Радиус сферы, охватывающей frustum
        float radius = 0.0f;
        for (int i = 0; i < 8; ++i) {
            float dist = (cascadeCorners[i] - frustumCenter).Length();
            radius = std::max(radius, dist);
        }

        // Увеличиваем радиус для PCF фильтрации
        float texelsPerUnit = CSM_SHADOW_MAP_SIZE / (radius * 2.0f);

        // Позиция света
        Vector3 lightPos = frustumCenter - lightDir * radius;

        // Создаём View матрицу для света
        Matrix lightView = Matrix::CreateLookAt(lightPos, frustumCenter, up);

        // Стабилизация теней (убираем дрожание)
        // Преобразуем центр frustum в light space
        Vector3 lightSpaceCenter = Vector3::Transform(frustumCenter, lightView);

        // Округляем до ближайшего текселя
        lightSpaceCenter.x = floor(lightSpaceCenter.x * texelsPerUnit) / texelsPerUnit;
        lightSpaceCenter.y = floor(lightSpaceCenter.y * texelsPerUnit) / texelsPerUnit;

        // Преобразуем обратно в world space
        Matrix invLightView = lightView.Invert();
        Vector3 roundedWorldCenter = Vector3::Transform(lightSpaceCenter, invLightView);
        Vector3 offset = frustumCenter - roundedWorldCenter;

        lightPos += offset;
        lightView = Matrix::CreateLookAt(lightPos, roundedWorldCenter, up);

        cascades[cascade].viewMatrix = lightView;

        // Ортографическая проекция для света
        Matrix lightProj = Matrix::CreateOrthographicOffCenter(
            -radius, radius, -radius, radius, 0.0f, radius * 2.0f);

        cascades[cascade].projMatrix = lightProj;
    }
}

void Game::PrepareCSMShadowPass(UINT cascade) {
    if (!Context || cascade >= CASCADE_COUNT) return;

    Context->ClearDepthStencilView(CSMShadowMapDSVs[cascade], D3D11_CLEAR_DEPTH, 1.0f, 0);

    ID3D11RenderTargetView* nullRTV[1] = { nullptr };
    Context->OMSetRenderTargets(0, nullRTV, CSMShadowMapDSVs[cascade]);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = (float)CSM_SHADOW_MAP_SIZE;
    viewport.Height = (float)CSM_SHADOW_MAP_SIZE;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    Context->RSSetViewports(1, &viewport);

    // Заполняем 4 каскада (для совместимости с шейдером, который ожидает массив из 4)
    // НО передаём правильную матрицу для ТЕКУЩЕГО каскада в свой слот
    // ShadowRenderer использует ТОЛЬКО slot [0] для lightView и lightProj
    ShadowConstantBuffer shadowCB;
    for (int i = 0; i < 4; i++) {
        shadowCB.lightView[i] = cascades[cascade].viewMatrix.Transpose();
        shadowCB.lightProjection[i] = cascades[cascade].projMatrix.Transpose();
    }
    Context->UpdateSubresource(shadowConstantBuffer, 0, nullptr, &shadowCB, 0, 0);
    Context->VSSetConstantBuffers(0, 1, &shadowConstantBuffer);
}

Matrix Game::GetCascadeLightViewMatrix(UINT cascade) const {
    return (cascade < CASCADE_COUNT) ? cascades[cascade].viewMatrix : Matrix::Identity;
}

Matrix Game::GetCascadeLightProjectionMatrix(UINT cascade) const {
    return (cascade < CASCADE_COUNT) ? cascades[cascade].projMatrix : Matrix::Identity;
}

float Game::GetCascadeSplitDepth(UINT cascade) const {
    return (cascade < CASCADE_COUNT) ? cascades[cascade].splitDepth : 0.0f;
}