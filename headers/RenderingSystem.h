// RenderingSystem.h
#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <SimpleMath.h>
#include "GBuffer.h"
#include "Lighting.h"
#include "Core.h"

using namespace DirectX::SimpleMath;

class Game;
struct Vertex;

class RenderingSystem {
public:
    RenderingSystem();
    ~RenderingSystem();

    RenderingSystem(const RenderingSystem&) = delete;
    RenderingSystem& operator=(const RenderingSystem&) = delete;

    HRESULT Initialize(Game* game, int width, int height);
    void Destroy();

    void BeginGeometryPass(ID3D11DeviceContext* context, const Matrix& view, const Matrix& projection);
    void DrawMeshToGBuffer(ID3D11DeviceContext* context, ID3D11Buffer* vertexBuffer, ID3D11Buffer* indexBuffer, UINT indexCount, const Matrix& world);
    void EndGeometryPass(ID3D11DeviceContext* context);

    void RenderLighting(ID3D11DeviceContext* context, ID3D11RenderTargetView* finalRTV, const DirectionalLight& light, const Vector3& cameraPosition);
    void RenderDebugGBuffer(ID3D11DeviceContext* context, ID3D11RenderTargetView* target, int textureIndex);

    void TestDrawRedScreen(ID3D11DeviceContext* context, ID3D11RenderTargetView* target);
    void RenderSimpleFullscreenQuad(ID3D11DeviceContext* context, ID3D11RenderTargetView* target);

    GBuffer* GetGBuffer() const { return gBuffer; }
    bool IsInitialized() const { return initialized; }

private:
    Game* game;
    GBuffer* gBuffer;
    int screenWidth;
    int screenHeight;
    bool initialized;

    Matrix currentView;
    Matrix currentProjection;

    ID3D11VertexShader* fullscreenVS;
    ID3D11VertexShader* geometryVS;
    ID3D11PixelShader* geometryPS;
    ID3D11PixelShader* directionalLightPS;
    ID3D11PixelShader* debugGBufferPS;

    ID3D11InputLayout* inputLayout;

    ID3D11Buffer* vsConstantBuffer;
    ID3D11Buffer* directionalLightBuffer;
    ID3D11Buffer* cameraBuffer;

    ID3D11SamplerState* linearSampler;
    ID3D11SamplerState* pointSampler;
    ID3D11BlendState* additiveBlendState;
    ID3D11BlendState* noBlendState;
    ID3D11DepthStencilState* lightDepthState;
    ID3D11RasterizerState* noCullRasterizer;

    ID3DBlob* CompileShader(const char* code, const char* target, const char* entry);
    HRESULT CreateShaders();
    HRESULT CreateBuffers();
    HRESULT CreateStates();

public:
    // Добавьте в public секцию:
    const Matrix& GetViewMatrix() const { return currentView; }
    const Matrix& GetProjectionMatrix() const { return currentProjection; }
};