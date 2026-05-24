// TexturedGround.h
#pragma once
#include "GameComponent.h"
#include "TextureLoader.h"
#include "ShadowRenderer.h"
#include "RenderingSystem.h"
#include <SimpleMath.h>
#include <vector>
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX::SimpleMath;

class TexturedGround : public GameComponent {
private:
    struct Vertex {
        Vector3 position;
        Vector2 texCoord;
        Vector3 normal;
        Vector4 color;
    };

    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11Buffer* vsConstantBuffer = nullptr;
    ID3D11Buffer* psConstantBuffer = nullptr;
    ID3D11Buffer* materialBuffer = nullptr;
    ID3D11Buffer* lightBuffer = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;

    UINT indexCount = 0;
    bool initialized = false;
    bool textureLoaded = false;
    float size;
    int segments;
    std::string texturePath;

    ID3DBlob* CompileShader(const char* code, const char* target, const char* entryPoint);
    void CreateGeometry();
    void CreateShaders();
    void CreateBuffers();

public:
    TexturedGround(Game* game, float size = 100.0f, int segments = 100, const std::string& textureFile = "models/wood.jpg");
    ~TexturedGround();

    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawGeometry(RenderingSystem* rs) override;
    void DrawShadow() override;
    void DestroyResources() override;

    // Добавьте в public секцию:
    void DebugTexture() {
        if (textureView) {
            std::cout << "Texture is valid, pointer: " << textureView << std::endl;
        }
        else {
            std::cout << "Texture is NULL!" << std::endl;
        }
    }
};