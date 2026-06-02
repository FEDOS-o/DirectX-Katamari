// GBuffer.h
#pragma once
#include <d3d11.h>

class GBuffer {
public:
    enum TextureType {
        DIFFUSE = 0,    // RGBA8 - цвет (RGB) + unused (A)
        NORMAL = 1,     // RGBA16F - нормали (XYZ) + unused (W)
        WORLD_POS = 2,  // RGBA16F - позиция в мире (XYZ) + unused (W)
        SPECULAR = 3,   // RGBA8 - specular цвет (RGB) + shininess (A)
        OBJECT_ID = 4,  // R32_UINT - ID объекта (32-бит целое)
        NUM_TEXTURES
    };

private:
    ID3D11Texture2D* textures[NUM_TEXTURES];
    ID3D11RenderTargetView* rtvs[NUM_TEXTURES];
    ID3D11ShaderResourceView* srvs[NUM_TEXTURES];

    ID3D11Texture2D* depthTexture;
    ID3D11DepthStencilView* depthDSV;
    ID3D11ShaderResourceView* depthSRV;

    int width;
    int height;
    bool initialized;

public:
    GBuffer();
    ~GBuffer();

    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;

    HRESULT Initialize(ID3D11Device* device, int w, int h);
    void Release();

    void SetRenderTargets(ID3D11DeviceContext* context);
    void Clear(ID3D11DeviceContext* context);
    ID3D11DepthStencilView* GetDepthDSV() const { return depthDSV; }

    ID3D11ShaderResourceView* GetSRV(TextureType type) const;
    ID3D11RenderTargetView* GetRTV(TextureType type) const {
        return (type >= 0 && type < NUM_TEXTURES) ? rtvs[type] : nullptr;
    }
    ID3D11ShaderResourceView* GetDepthSRV() const { return depthSRV; }

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    bool IsInitialized() const { return initialized; }
};