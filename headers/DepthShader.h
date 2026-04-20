#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <fstream>

using namespace DirectX;

class DepthShader {
private:
    struct MatrixBufferType {
        XMMATRIX world;
        XMMATRIX view;
        XMMATRIX projection;
    };

public:
    DepthShader();
    ~DepthShader();

    bool Initialize(ID3D11Device* device, HWND hwnd);
    void Shutdown();
    bool Render(ID3D11DeviceContext* context, int indexCount, XMMATRIX world, XMMATRIX view, XMMATRIX projection);

private:
    bool InitializeShader(ID3D11Device* device, HWND hwnd, const wchar_t* vsFile, const wchar_t* psFile);
    void ShutdownShader();
    void OutputShaderErrorMessage(ID3D10Blob* error, HWND hwnd, const wchar_t* file);
    bool SetShaderParameters(ID3D11DeviceContext* context, XMMATRIX world, XMMATRIX view, XMMATRIX projection);
    void RenderShader(ID3D11DeviceContext* context, int indexCount);

private:
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_layout = nullptr;
    ID3D11Buffer* m_matrixBuffer = nullptr;
};