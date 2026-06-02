// Game.cpp
#include "Game.h"
#include "Core.h"
#include "OrbitalCamera.h"
#include "FirstPersonCamera.h"
#include "DisplayWin32.h"
#include "InputDevice.h"
#include "ShadowRenderer.h"
#include "RenderingSystem.h"
#include <iostream>
#include <algorithm>
#include <Skybox.h>
#include <Particle.h>
#include <KatamariBall.h>

// Game.cpp - Обновленный конструктор
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
    DepthStencilState(nullptr),
    renderingSystem(nullptr)
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

    // Создаем Directional Light и добавляем в систему
    DirectionalLightComponent* mainLight = new DirectionalLightComponent(this,
        Vector3(0.5f, -1.0f, 0.3f), Vector4(1, 1, 1, 1), 1.0f, true);
    AddLight(mainLight);

    // Обновляем SunLight для обратной совместимости с существующим кодом
    SunLight.direction = mainLight->GetDirection();
    SunLight.ambient = Vector4(0.15f, 0.15f, 0.15f, 1.0f);
    SunLight.diffuse = Vector4(0.9f, 0.9f, 0.9f, 1.0f);
    SunLight.specular = Vector4(0.3f, 0.3f, 0.3f, 1.0f);

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
    if (FAILED(res)) return res;

    res = CreateDepthBuffer();
    if (FAILED(res)) return res;

    renderingSystem = new RenderingSystem();
    res = renderingSystem->Initialize(this, 800, 800);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to initialize RenderingSystem", L"Error", MB_OK);
        return res;
    }

    res = CreateShadowMapResources();
    if (FAILED(res)) return res;

    res = CreateCSMResources();
    if (FAILED(res)) return res;

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

    if (skybox) {
        skybox->Initialize();
    }

    ShadowRendererComp = new Render::ShadowRenderer();
    ShadowRendererComp->Initialize(this);

    // Инициализируем все добавленные компоненты
    for (auto* component : components) {
        component->Initialize();
    }

    return S_OK;
}

// В Game.cpp, в методе Update() добавить:

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

    // ============================================
    // PIXEL PICKING - обработка клика мыши
    // ============================================
    static bool leftMouseWasPressed = false;
    if (Input && Input->IsKeyDown(Keys::LeftButton)) {
        if (!leftMouseWasPressed) {
            uint32_t pickedId = PickObjectAtMousePosition();
            if (pickedId != 0) {
                GameComponent* pickedComponent = GetComponentById(pickedId);
                if (pickedComponent) {
                    std::cout << "Picked object with ID: " << pickedId << std::endl;

                    // Можно проверить тип объекта
                    Prop* pickedProp = dynamic_cast<Prop*>(pickedComponent);
                    if (pickedProp) {
                        std::cout << "  -> It's a Prop at position: "
                            << pickedProp->GetPosition().x << ", "
                            << pickedProp->GetPosition().y << ", "
                            << pickedProp->GetPosition().z << std::endl;
                    }

                    KatamariBall* pickedBall = dynamic_cast<KatamariBall*>(pickedComponent);
                    if (pickedBall) {
                        std::cout << "  -> It's the Katamari Ball! Radius: "
                            << pickedBall->GetRadius()
                            << ", Attached objects: " << pickedBall->GetAttachedCount() << std::endl;
                    }
                }
            }
            else {
                std::cout << "Picked nothing (ID = 0)" << std::endl;
            }
            leftMouseWasPressed = true;
        }
    }
    else {
        leftMouseWasPressed = false;
    }

    Camera->Update(deltaTime);

    for (auto* component : components) {
        component->Update(deltaTime);
    }

    UpdateLights(deltaTime);
    UpdateAnimatedLights(deltaTime);
    UpdateLight(deltaTime);
    UpdateInternal(deltaTime);
}

void Game::UpdateInternal(float deltaTime) {
    (void)deltaTime;
}

void Game::PrepareFrame() {
    Context->ClearState();

    if (ScreenResized) {
        RestoreTargets();
        ScreenResized = false;
    }
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
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    Context->ClearRenderTargetView(RenderView, clearColor);
    Context->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // ============================================
    // SHADOW PASS
    // ============================================
    UpdateCascades();
    for (UINT cascade = 0; cascade < CASCADE_COUNT; ++cascade) {
        PrepareCSMShadowPass(cascade);
        if (ShadowRendererComp) ShadowRendererComp->BeginShadowPass(this);

        ShadowConstantBuffer shadowCB;
        for (int i = 0; i < 4; i++) {
            shadowCB.lightView[i] = cascades[cascade].viewMatrix.Transpose();
            shadowCB.lightProjection[i] = cascades[cascade].projMatrix.Transpose();
        }
        Context->UpdateSubresource(shadowConstantBuffer, 0, nullptr, &shadowCB, 0, 0);
        Context->VSSetConstantBuffers(0, 1, &shadowConstantBuffer);

        for (auto* component : components) component->DrawShadow();
        if (ShadowRendererComp) ShadowRendererComp->EndShadowPass(this);
    }

    // ============================================
    // GEOMETRY PASS
    // ============================================
    renderingSystem->BeginGeometryPass(Context, Camera->GetViewMatrix(), Camera->GetProjectionMatrix());
    for (auto* component : components) {
        component->DrawGeometry(renderingSystem);
    }
    renderingSystem->EndGeometryPass(Context);


    static bool savedOnce = false;
    if (!savedOnce) {
        SaveObjectIdTextureToFile("object_id_debug.ppm");
        savedOnce = true;
    }
    // ============================================
    // LIGHTING PASS
    // ============================================
    float blendFactor[4] = { 0, 0, 0, 0 };
    Context->OMSetBlendState(renderingSystem->GetAdditiveBlendState(), blendFactor, 0xffffffff);
    Context->OMSetRenderTargets(1, &RenderView, renderingSystem->GetGBuffer()->GetDepthDSV());

    renderingSystem->RenderLighting(Context, RenderView, SunLight, Camera->GetPosition(),
        CSMShadowMapSRVs[0], ShadowSampler);

    Context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);

    // ============================================
    // SKYBOX
    // ============================================
    if (skybox) skybox->Draw();

    // ============================================
    // FORWARD PASS (частицы и всё прозрачное)
    // ============================================
    D3D11_DEPTH_STENCIL_DESC forwardDSDesc = {};
    forwardDSDesc.DepthEnable = TRUE;
    forwardDSDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    forwardDSDesc.DepthFunc = D3D11_COMPARISON_LESS;
    forwardDSDesc.StencilEnable = FALSE;

    ID3D11DepthStencilState* forwardDepthState = nullptr;
    Device->CreateDepthStencilState(&forwardDSDesc, &forwardDepthState);

    Context->OMSetRenderTargets(1, &RenderView, renderingSystem->GetGBuffer()->GetDepthDSV());
    Context->OMSetDepthStencilState(forwardDepthState, 0);

    for (auto* component : components) component->Draw();

    if (forwardDepthState) forwardDepthState->Release();
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

    // Уничтожаем все компоненты (Game владеет ими)
    for (auto* component : components) {
        component->DestroyResources();
        delete component;
    }
    components.clear();

    // Уничтожаем все компоненты света
    for (auto* light : lightComponents) {
        delete light;
    }
    lightComponents.clear();
    lightingSystem.Clear();

    if (renderingSystem) {
        renderingSystem->Destroy();
        delete renderingSystem;
        renderingSystem = nullptr;
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

void Game::UpdateAnimatedLights(float deltaTime) {
    static float time = 0;
    time += deltaTime;

    for (auto* light : lightComponents) {
        PointLightComponent* pointLight = dynamic_cast<PointLightComponent*>(light);
        if (!pointLight) continue;

        Vector3 pos = pointLight->GetPosition();

        if (pointLight->GetColor().x > 0.9f && pointLight->GetColor().y < 0.4f) {
            float radius = 3.5f;
            float speed = 1.5f;
            float newX = cos(time * speed) * radius;
            float newZ = sin(time * speed) * radius;
            pointLight->SetPosition(Vector3(newX, 1.5f + sin(time * 3.0f) * 0.5f, newZ));
        }

        if (pointLight->GetColor().x > 0.9f && pointLight->GetColor().y > 0.9f) {
            float intensity = 0.6f + sin(time * 15.0f) * 0.3f;
            intensity = std::max(0.3f, std::min(1.2f, intensity));
            pointLight->SetIntensity(intensity);
        }
    }
}

// ===== SHADOW MAP METHODS =====

HRESULT Game::CreateShadowMapResources() {
    if (!Device) return E_FAIL;

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

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    hr = Device->CreateDepthStencilView(ShadowMapTexture, &dsvDesc, &ShadowMapDSV);
    if (FAILED(hr)) return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    hr = Device->CreateShaderResourceView(ShadowMapTexture, &srvDesc, &ShadowMapSRV);
    if (FAILED(hr)) return hr;

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

    Vector3 lightPos = Vector3(-20.0f, 30.0f, -20.0f);
    Vector3 lightTarget = Vector3(0.0f, 2.0f, 0.0f);
    Vector3 up = Vector3(0, 1, 0);

    Vector3 lightDir = SunLight.direction;
    lightDir.Normalize();
    lightPos = lightTarget - lightDir * 50.0f;

    lightViewMatrix = Matrix::CreateLookAt(lightPos, lightTarget, up);

    float orthoSize = 40.0f;
    float nearPlane = 1.0f;
    float farPlane = 100.0f;
    lightProjectionMatrix = Matrix::CreateOrthographic(orthoSize, orthoSize, nearPlane, farPlane);

    ShadowConstantBuffer shadowCB;
    for (int i = 0; i < 4; i++) {
        shadowCB.lightView[i] = lightViewMatrix.Transpose();
        shadowCB.lightProjection[i] = lightProjectionMatrix.Transpose();
    }
    Context->UpdateSubresource(shadowConstantBuffer, 0, nullptr, &shadowCB, 0, 0);
    Context->VSSetConstantBuffers(0, 1, &shadowConstantBuffer);
}

void Game::SetShadowForRender() {
}

HRESULT Game::CreateShadowShaders() {
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

// ===== CSM METHODS =====

HRESULT Game::CreateCSMResources() {
    if (!Device) return E_FAIL;

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

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDescAll = {};
    srvDescAll.Format = DXGI_FORMAT_R32_FLOAT;
    srvDescAll.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDescAll.Texture2DArray.MostDetailedMip = 0;
    srvDescAll.Texture2DArray.MipLevels = 1;
    srvDescAll.Texture2DArray.FirstArraySlice = 0;
    srvDescAll.Texture2DArray.ArraySize = CASCADE_COUNT;

    hr = Device->CreateShaderResourceView(CSMShadowMapTexture, &srvDescAll, &CSMShadowMapSRVs[0]);
    if (FAILED(hr)) return hr;

    for (UINT i = 1; i < CASCADE_COUNT; ++i) {
        CSMShadowMapSRVs[i] = nullptr;
    }

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.ByteWidth = sizeof(CSMConstantBuffer);
    hr = Device->CreateBuffer(&cbDesc, nullptr, &csmConstantBuffer);

    return hr;
}

void Game::UpdateCascades() {
    float camNear = 0.5f;
    float camFar = 200.0f;
    Matrix camView = Camera->GetViewMatrix();
    Matrix camProj = Camera->GetProjectionMatrix();

    float splits[CASCADE_COUNT];
    for (UINT i = 0; i < CASCADE_COUNT; ++i) {
        float p = (float)(i + 1) / CASCADE_COUNT;
        float logSplit = camNear * pow(camFar / camNear, p);
        float uniformSplit = camNear + (camFar - camNear) * p;
        splits[i] = cascadeSplitLambda * logSplit + (1.0f - cascadeSplitLambda) * uniformSplit;
    }

    Matrix invCamViewProj = (camView * camProj).Invert();

    Vector3 lightDir = SunLight.direction;
    lightDir.Normalize();

    Vector3 up = Vector3(0, 1, 0);
    if (abs(lightDir.Dot(up)) > 0.999f) {
        up = Vector3(1, 0, 0);
    }

    for (UINT cascade = 0; cascade < CASCADE_COUNT; ++cascade) {
        float cascadeNear = (cascade == 0) ? camNear : splits[cascade - 1];
        float cascadeFar = splits[cascade];

        cascades[cascade].splitDepth = cascadeFar;

        Vector3 frustumCorners[8];
        Vector4 ndcCorners[8] = {
            Vector4(-1, -1, 0, 1), Vector4(1, -1, 0, 1),
            Vector4(1,  1, 0, 1), Vector4(-1,  1, 0, 1),
            Vector4(-1, -1, 1, 1), Vector4(1, -1, 1, 1),
            Vector4(1,  1, 1, 1), Vector4(-1,  1, 1, 1)
        };

        for (int i = 0; i < 8; ++i) {
            Vector4 worldPos = Vector4::Transform(ndcCorners[i], invCamViewProj);
            frustumCorners[i] = Vector3(worldPos.x, worldPos.y, worldPos.z) / worldPos.w;
        }

        Vector3 nearCorners[4], farCorners[4];
        for (int i = 0; i < 4; ++i) {
            nearCorners[i] = frustumCorners[i];
            farCorners[i] = frustumCorners[i + 4];
        }

        Vector3 cascadeCorners[8];
        for (int i = 0; i < 4; ++i) {
            float nearT = (cascadeNear - camNear) / (camFar - camNear);
            float farT = (cascadeFar - camNear) / (camFar - camNear);
            cascadeCorners[i] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * nearT;
            cascadeCorners[i + 4] = nearCorners[i] + (farCorners[i] - nearCorners[i]) * farT;
        }

        Vector3 frustumCenter = Vector3::Zero;
        for (int i = 0; i < 8; ++i) {
            frustumCenter += cascadeCorners[i];
        }
        frustumCenter /= 8.0f;

        float radius = 0.0f;
        for (int i = 0; i < 8; ++i) {
            float dist = (cascadeCorners[i] - frustumCenter).Length();
            radius = std::max(radius, dist);
        }

        float texelsPerUnit = CSM_SHADOW_MAP_SIZE / (radius * 2.0f);

        Vector3 lightPos = frustumCenter - lightDir * radius;
        Matrix lightView = Matrix::CreateLookAt(lightPos, frustumCenter, up);

        Vector3 lightSpaceCenter = Vector3::Transform(frustumCenter, lightView);
        lightSpaceCenter.x = floor(lightSpaceCenter.x * texelsPerUnit) / texelsPerUnit;
        lightSpaceCenter.y = floor(lightSpaceCenter.y * texelsPerUnit) / texelsPerUnit;

        Matrix invLightView = lightView.Invert();
        Vector3 roundedWorldCenter = Vector3::Transform(lightSpaceCenter, invLightView);
        Vector3 offset = frustumCenter - roundedWorldCenter;

        lightPos += offset;
        lightView = Matrix::CreateLookAt(lightPos, roundedWorldCenter, up);

        cascades[cascade].viewMatrix = lightView;

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

void Game::UpdateLight(float deltaTime) {
    DirectionalLightComponent* mainDir = GetMainDirectionalLight();
    if (mainDir) {
        SunLight.direction = mainDir->GetDirection();

        static float lightAngle = 0.0f;
        lightAngle += deltaTime * 0.1f;

        Vector3 newDir = Vector3(sin(lightAngle) * 0.5f, -1.0f, cos(lightAngle) * 0.5f);
        newDir.Normalize();
        mainDir->SetDirection(newDir);
    }
}

void Game::RenderSceneToShadowMap() {
}

// ===== COMPONENT MANAGEMENT METHODS =====

void Game::AddComponent(GameComponent* component) {
    if (!component) return;

    uint32_t newId = nextComponentId++;
    component->SetId(newId);

    components.push_back(component);

    if (renderingSystem && renderingSystem->IsInitialized()) {
        component->Initialize();
    }
}

void Game::RemoveComponent(GameComponent* component) {
    if (!component) return;

    auto it = std::remove(components.begin(), components.end(), component);
    if (it != components.end()) {
        components.erase(it, components.end());
    }

    component->DestroyResources();
}

void Game::RemoveComponentById(uint32_t id) {
    GameComponent* comp = GetComponentById(id);
    if (comp) {
        RemoveComponent(comp);
    }
}

GameComponent* Game::GetComponentById(uint32_t id) const {
    for (auto* comp : components) {
        if (comp && comp->GetId() == id) {
            return comp;
        }
    }
    return nullptr;
}


// Game.cpp - добавить в конец файла

uint32_t Game::PickObjectAtScreenPos(int screenX, int screenY) {
    if (!renderingSystem || !renderingSystem->GetGBuffer()) return 0;

    GBuffer* gbuffer = renderingSystem->GetGBuffer();
    ID3D11ShaderResourceView* idSRV = gbuffer->GetSRV(GBuffer::OBJECT_ID);
    if (!idSRV) {
        std::cout << "PickObject: idSRV is NULL!" << std::endl;
        return 0;
    }

    ID3D11Texture2D* idTexture = nullptr;
    idSRV->GetResource((ID3D11Resource**)&idTexture);
    if (!idTexture) {
        std::cout << "PickObject: idTexture is NULL!" << std::endl;
        return 0;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    idTexture->GetDesc(&texDesc);

    std::cout << "Texture size: " << texDesc.Width << "x" << texDesc.Height
        << ", Format: " << texDesc.Format << std::endl;

    D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D* stagingTexture = nullptr;
    HRESULT hr = Device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        std::cout << "PickObject: Failed to create staging texture, HR=" << std::hex << hr << std::endl;
        idTexture->Release();
        return 0;
    }

    Context->CopyResource(stagingTexture, idTexture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = Context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        std::cout << "PickObject: Failed to map staging texture, HR=" << std::hex << hr << std::endl;
        stagingTexture->Release();
        idTexture->Release();
        return 0;
    }

    // Проверим несколько пикселей для отладки
    uint32_t* data = (uint32_t*)mapped.pData;
    int pitch = mapped.RowPitch / sizeof(uint32_t);

    int texX = screenX;
    int texY = screenY;
    uint32_t pickedId = 0;

    if (texX >= 0 && texX < (int)texDesc.Width && texY >= 0 && texY < (int)texDesc.Height) {
        pickedId = data[texY * pitch + texX];
        std::cout << "Picked at mouse (" << texX << "," << texY << "): ID = " << pickedId << std::endl;
    }
    else {
        std::cout << "Mouse coordinates out of bounds: (" << texX << "," << texY << ")" << std::endl;
    }

    Context->Unmap(stagingTexture, 0);
    stagingTexture->Release();
    idTexture->Release();

    return pickedId;
}

uint32_t Game::PickObjectAtMousePosition() {
    if (!Input) return 0;

    POINT mousePos;
    GetCursorPos(&mousePos);
    ScreenToClient(Display->Window, &mousePos);

    return PickObjectAtScreenPos(mousePos.x, mousePos.y);
}


void Game::SaveObjectIdTextureToFile(const char* filename) {
    if (!renderingSystem || !renderingSystem->GetGBuffer()) {
        std::cout << "SaveObjectIdTexture: No GBuffer!" << std::endl;
        return;
    }

    GBuffer* gbuffer = renderingSystem->GetGBuffer();
    ID3D11ShaderResourceView* idSRV = gbuffer->GetSRV(GBuffer::OBJECT_ID);
    if (!idSRV) {
        std::cout << "SaveObjectIdTexture: idSRV is NULL!" << std::endl;
        return;
    }

    ID3D11Texture2D* idTexture = nullptr;
    idSRV->GetResource((ID3D11Resource**)&idTexture);
    if (!idTexture) {
        std::cout << "SaveObjectIdTexture: idTexture is NULL!" << std::endl;
        return;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    idTexture->GetDesc(&texDesc);

    std::cout << "Texture: " << texDesc.Width << "x" << texDesc.Height
        << ", Format: " << texDesc.Format << std::endl;

    // Создаем staging текстуру
    D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ID3D11Texture2D* stagingTexture = nullptr;
    HRESULT hr = Device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        std::cout << "Failed to create staging texture, HR=" << std::hex << hr << std::endl;
        idTexture->Release();
        return;
    }

    Context->CopyResource(stagingTexture, idTexture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = Context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        std::cout << "Failed to map texture, HR=" << std::hex << hr << std::endl;
        stagingTexture->Release();
        idTexture->Release();
        return;
    }

    uint32_t* data = (uint32_t*)mapped.pData;
    int pitch = mapped.RowPitch / sizeof(uint32_t);

    // Создаем простой PPM файл для просмотра (визуализация ID как цвета)
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "P3\n" << texDesc.Width << " " << texDesc.Height << "\n255\n";

        int nonZeroCount = 0;
        int totalPixels = texDesc.Width * texDesc.Height;

        for (UINT y = 0; y < texDesc.Height; y++) {
            for (UINT x = 0; x < texDesc.Width; x++) {
                uint32_t id = data[y * pitch + x];
                if (id != 0) nonZeroCount++;

                // Преобразуем ID в цвет (простейший способ увидеть ненулевые значения)
                int r = (id & 0xFF);
                int g = ((id >> 8) & 0xFF);
                int b = ((id >> 16) & 0xFF);

                if (id == 0) {
                    file << "0 0 0 ";
                }
                else {
                    file << r << " " << g << " " << b << " ";
                }
            }
            file << "\n";
        }
        file.close();

        std::cout << "Saved to " << filename << std::endl;
        std::cout << "Non-zero pixels: " << nonZeroCount << " / " << totalPixels
            << " (" << (nonZeroCount * 100.0f / totalPixels) << "%)" << std::endl;

        // Выведем первые несколько значений для отладки
        std::cout << "First 20 pixel values:" << std::endl;
        for (int i = 0; i < 20 && i < totalPixels; i++) {
            std::cout << data[i] << " ";
            if ((i + 1) % 10 == 0) std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    Context->Unmap(stagingTexture, 0);
    stagingTexture->Release();
    idTexture->Release();
}