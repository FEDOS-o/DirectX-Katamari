#pragma once

#include <windows.h>
#include <WinUser.h>
#include <wrl.h>
#include <iostream>
#include <d3d.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <chrono>
#include <cstdlib>
#include <vector>
#include "DisplayWin32.h"
#include "GameComponent.h"
#include "InputDevice.h"


#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

class OrbitalCameraGameComponent;
class FirstPersonCameraGameComponent;
class Camera;
class GameComponent;

class Game {
private:
    HINSTANCE Instance;
    LPCWSTR Name;

    IDXGISwapChain* SwapChain;
    ID3D11Texture2D* BackBuffer;
    ID3D11UnorderedAccessView* RenderSRV;
    ID3D11Debug* DebugAnnotation;
    ID3D11RasterizerState* RasterizerState;

    ID3D11Texture2D* DepthStencilBuffer;
    ID3D11DepthStencilView* DepthStencilView;
    ID3D11DepthStencilState* DepthStencilState;

    std::chrono::time_point<std::chrono::steady_clock> PrevTime;
    std::chrono::time_point<std::chrono::steady_clock> StartTime;
 

    bool isUsingOrbitalCamera;
    bool ScreenResized;

public:

    float TotalTime;

    OrbitalCameraGameComponent* OrbitalCamera;
    FirstPersonCameraGameComponent* FirstPersonCamera;
    Camera* Camera;


    ID3D11RenderTargetView* RenderView;
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    ID3D11DeviceContext* Context;

    InputDevice* Input;
    DisplayWin32* Display;
    std::vector<GameComponent*> components;

public:
    Game(LPCWSTR applicationName, HINSTANCE hInstance, LONG screenWidth, LONG screenHeight);
    ~Game();

    HRESULT CreateBackBuffer();
    HRESULT CreateDepthBuffer();
    HRESULT Initialize();

    void Update();
    void UpdateInternal(float deltaTime);
    void PrepareFrame();
    void PrepareResources();
    void RestoreTargets();
    void Draw();
    void EndFrame();
    void MessageHandler(MSG& msg);
    void Exit();
    void DestroyResources();
    void Run();
    void SwitchCamera();
};