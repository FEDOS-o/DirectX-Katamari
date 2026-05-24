// Game.h (исправленный)
#pragma once

#include <windows.h>
#include <WinUser.h>
#include <wrl.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <chrono>
#include <vector>
#include "DisplayWin32.h"
#include "GameComponent.h"
#include "InputDevice.h"
#include "Lighting.h"
#include "Core.h"
#include "GBuffer.h"
#include "RenderingSystem.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

class OrbitalCamera;
class FirstPersonCamera;
class Camera;
class GameComponent;
namespace Render {
    class ShadowRenderer;
}

class Game {
private:
    HINSTANCE Instance;
    LPCWSTR Name;

    IDXGISwapChain* SwapChain = nullptr;
    ID3D11Texture2D* BackBuffer = nullptr;
    ID3D11UnorderedAccessView* RenderSRV = nullptr;
    ID3D11Debug* DebugAnnotation = nullptr;

    // Shadow map resources (private, но есть public геттеры если нужны)
    ID3D11Texture2D* ShadowMapTexture = nullptr;
    ID3D11DepthStencilView* ShadowMapDSV = nullptr;

public:
    // ƒелаем public дл€ доступа из RenderingSystem
    ID3D11ShaderResourceView* ShadowMapSRV = nullptr;

    ID3D11RasterizerState* RasterizerState = nullptr;

    ID3D11Texture2D* DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView* DepthStencilView = nullptr;
    ID3D11DepthStencilState* DepthStencilState = nullptr;

    std::chrono::time_point<std::chrono::steady_clock> PrevTime;
    std::chrono::time_point<std::chrono::steady_clock> StartTime;

    bool isUsingOrbitalCamera = true;
    bool ScreenResized = false;

public:
    float TotalTime = 0.0f;

    OrbitalCamera* orbitalCamera = nullptr;
    FirstPersonCamera* firstPersonCamera = nullptr;
    Camera* Camera = nullptr;

    ID3D11RenderTargetView* RenderView = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device> Device;
    ID3D11DeviceContext* Context = nullptr;

    InputDevice* Input = nullptr;
    DisplayWin32* Display = nullptr;
    std::vector<GameComponent*> components;

    DirectionalLight SunLight;
    ID3D11ShaderResourceView* SkyboxTexture = nullptr;

    // Deferred Rendering System
    RenderingSystem* renderingSystem = nullptr;

    // Shadow resources
    ID3D11SamplerState* ShadowSampler = nullptr;
    ID3D11Buffer* shadowConstantBuffer = nullptr;
    static constexpr UINT SHADOW_MAP_SIZE = 4096;
    float ShadowBias = 0.00005f;
    float ShadowBiasSlope = 2.0f;

    Vector3 LightDirection = Vector3(0.5f, -1.0f, 0.3f);
    Vector3 LightTarget = Vector3(0, 0, 0);
    float LightDistance = 50.0f;

    Matrix lightViewMatrix;
    Matrix lightProjectionMatrix;

    // ћетоды
    void UpdateLight(float deltaTime);  // ƒќЅј¬Ћя≈ћ ќЅЏя¬Ћ≈Ќ»≈

    HRESULT CreateShadowMapResources();
    void PrepareShadowPass();
    void SetShadowForRender();
    Matrix GetLightViewMatrix() const;
    Matrix GetLightProjectionMatrix() const;

    // CSM resources
    static constexpr UINT CASCADE_COUNT = 4;
    static constexpr UINT CSM_SHADOW_MAP_SIZE = 2048;

    ID3D11Texture2D* CSMShadowMapTexture = nullptr;
    ID3D11DepthStencilView* CSMShadowMapDSVs[CASCADE_COUNT] = {};
    ID3D11ShaderResourceView* CSMShadowMapSRVs[CASCADE_COUNT] = {};

    struct CascadeData {
        float splitDepth;
        Matrix viewMatrix;
        Matrix projMatrix;
    };
    CascadeData cascades[CASCADE_COUNT];

    float cascadeSplitLambda = 0.95f;

    struct CSMConstantBuffer {
        Matrix lightViewProj[4];
        Vector4 cascadeSplits;
    };
    ID3D11Buffer* csmConstantBuffer = nullptr;

    HRESULT CreateCSMResources();
    void UpdateCascades();
    void PrepareCSMShadowPass(UINT cascade);
    Matrix GetCascadeLightViewMatrix(UINT cascade) const;
    Matrix GetCascadeLightProjectionMatrix(UINT cascade) const;
    float GetCascadeSplitDepth(UINT cascade) const;

    ID3D11VertexShader* ShadowVertexShader = nullptr;
    ID3D11PixelShader* ShadowPixelShader = nullptr;
    ID3D11InputLayout* ShadowInputLayout = nullptr;

    HRESULT CreateShadowShaders();
    void RenderSceneToShadowMap();  // ƒќЅј¬Ћя≈ћ ќЅЏя¬Ћ≈Ќ»≈

    Render::ShadowRenderer* ShadowRendererComp = nullptr;
    ID3D11Buffer* shadowWorldConstantBuffer = nullptr;

    void SetShadowWorldMatrix(const Matrix& world);

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