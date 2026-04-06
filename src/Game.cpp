#include "Game.h"
#include "OrbitalCameraGameComponent.h"
#include "FirstPersonCameraGameComponent.h"
#include "DisplayWin32.h"
#include "InputDevice.h"

Game::Game(LPCWSTR applicationName, HINSTANCE hInstance, LONG screenWidth, LONG screenHeight) :
    Instance(hInstance),
    Name(applicationName),
    TotalTime(0.0f),
    ScreenResized(false),
    Display(nullptr),
    Input(nullptr),
    isUsingOrbitalCamera(true),
    FirstPersonCamera(nullptr),
    OrbitalCamera(nullptr),
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

    OrbitalCamera = new OrbitalCameraGameComponent(this, Vector3(0, 0, 0), 15.0f);
    FirstPersonCamera = new FirstPersonCameraGameComponent(this, Vector3(0, 5, 15));
    Camera = OrbitalCamera;

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
    // Release existing resources if any
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

    // Create depth stencil state with depth test enabled
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

    CD3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.DepthClipEnable = true;

    res = Device->CreateRasterizerState(&rastDesc, &RasterizerState);
    if (FAILED(res)) {
        Display->createMessageBox(L"Failed to create rasterizer state", L"Error", MB_OK);
        return res;
    }

    OrbitalCamera->Initialize();
    FirstPersonCamera->Initialize();

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
        cWasPressed = false;
        if (!cWasPressed) {
            SwitchCamera();
            cWasPressed = true;
        }
    } else {
        cWasPressed = false;
    }


    Camera->Update(deltaTime);

    for (auto* component : components) {
        component->Update(deltaTime);
    }

    UpdateInternal(deltaTime);
}

void Game::UpdateInternal(float deltaTime) {
    // Empty implementation
}

void Game::PrepareFrame() {
    Context->ClearState();

    if (ScreenResized) {
        RestoreTargets();
        ScreenResized = false;
    }

    if (DepthStencilState) {
        Context->OMSetDepthStencilState(DepthStencilState, 1);
    }

    if (RasterizerState) {
        Context->RSSetState(RasterizerState);
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
    // Empty implementation
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
    float clearColor[] = { 0.05f, 0.05f, 0.1f, 1.0f };
    Context->ClearRenderTargetView(RenderView, clearColor);
    Context->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    Context->OMSetRenderTargets(1, &RenderView, DepthStencilView);
    
    Context->OMSetDepthStencilState(DepthStencilState, 1);
    Camera->Draw();

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
    if (OrbitalCamera) {
        OrbitalCamera->DestroyResources();
        delete OrbitalCamera;
        OrbitalCamera = nullptr;
    }
    if (FirstPersonCamera) {
        FirstPersonCamera->DestroyResources();
        delete FirstPersonCamera;
        FirstPersonCamera = nullptr;
    }

    for (auto* component : components) {
        component->DestroyResources();
    }

    if (DepthStencilState) {
        DepthStencilState->Release();
        DepthStencilState = nullptr;
    }
    if (DepthStencilView) {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }
    if (DepthStencilBuffer) {
        DepthStencilBuffer->Release();
        DepthStencilBuffer = nullptr;
    }
    if (RasterizerState) {
        RasterizerState->Release();
        RasterizerState = nullptr;
    }
    if (RenderView) {
        RenderView->Release();
        RenderView = nullptr;
    }
    if (BackBuffer) {
        BackBuffer->Release();
        BackBuffer = nullptr;
    }
    if (RenderSRV) {
        RenderSRV->Release();
        RenderSRV = nullptr;
    }
    if (DebugAnnotation) {
        DebugAnnotation->Release();
        DebugAnnotation = nullptr;
    }
    if (Context) {
        Context->Release();
        Context = nullptr;
    }
    if (SwapChain) {
        SwapChain->Release();
        SwapChain = nullptr;
    }
    if (Display) {
        delete Display;
        Display = nullptr;
    }
    if (Input) {
        delete Input;
        Input = nullptr;
    }
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
        Vector3 fpsPos = FirstPersonCamera->GetPosition();
        Vector3 fpsForward = FirstPersonCamera->GetForward();
        OrbitalCamera->SetTarget(fpsPos);
        OrbitalCamera->ResetCamera();
        Camera = OrbitalCamera;
    } else {
        Vector3 orbitalPos = Vector3(0, 5, 15); 
        FirstPersonCamera->SetPosition(orbitalPos);
        FirstPersonCamera->ResetCamera();
        Camera = FirstPersonCamera;
    }
}