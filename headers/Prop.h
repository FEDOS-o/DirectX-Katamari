#pragma once
#include "GameComponent.h"
#include "Model3D.h"

class Prop : public GameComponent {
private:
    Model3D model;
    Vector3 position;
    std::string modelPath;
    float modelScale;
    float collisionRadius = 0.5f;
    Vector3 collisionCenterOffset = Vector3::Zero;

    ID3D11Buffer* debugVertexBuffer = nullptr;
    ID3D11Buffer* debugIndexBuffer = nullptr;
    ID3D11VertexShader* debugVS = nullptr;
    ID3D11PixelShader* debugPS = nullptr;
    ID3D11InputLayout* debugLayout = nullptr;
    ID3D11Buffer* debugConstantBuffer = nullptr;
    bool debugInitialized = false;

    void InitDebugCollider();
    void DrawDebugCollider();
    ID3DBlob* CompileDebugShader(const char* code, const char* target, const char* entryPoint);

public:
    Prop(Game* game, const std::string& path, const Vector3& startPosition = Vector3(0, 0, 0), float scale = 0.15f);
    ~Prop();

    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DestroyResources() override;

    Model3D& GetModel() { return model; }
    void SetPosition(const Vector3& pos);
    Vector3 GetPosition() const { return position; }

    void UpdateCollisionData();
    Vector3 GetCollisionCenter() const { return position + collisionCenterOffset; }
    float GetCollisionRadius() const { return collisionRadius; }
};