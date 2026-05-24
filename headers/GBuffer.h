// GBuffer.h
#pragma once
#include <d3d11.h>

class GBuffer {
public:
    enum TextureType {
        DIFFUSE = 0,  // RGBA8 - цвет (RGB) + unused (A)
        NORMAL = 1,  // RGBA16F - нормали (XYZ) + unused (W)
        WORLD_POS = 2,  // RGBA16F - позиция в мире (XYZ) + unused (W)
        SPECULAR = 3,  // RGBA8 - specular цвет (RGB) + shininess (A)
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

    // Запрещаем копирование
    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;

    HRESULT Initialize(ID3D11Device* device, int w, int h);
    void Release();

    // Для GEOMETRY PASS (запись)
    void SetRenderTargets(ID3D11DeviceContext* context);
    void Clear(ID3D11DeviceContext* context);
    ID3D11DepthStencilView* GetDepthDSV() const { return depthDSV; }

    // Для LIGHTING PASS (чтение)
    ID3D11ShaderResourceView* GetSRV(TextureType type) const;
    void TestClearColors(ID3D11DeviceContext* context);
    ID3D11ShaderResourceView* GetDepthSRV() const { return depthSRV; }

    // Геттеры
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    bool IsInitialized() const { return initialized; }

    // GBuffer.h - добавить в public секцию:
    ID3D11RenderTargetView* GetRTV(int index) const {
        return (index >= 0 && index < NUM_TEXTURES) ? rtvs[index] : nullptr;
    }
};