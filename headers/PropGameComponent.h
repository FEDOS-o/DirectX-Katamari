#pragma once
#include "GameComponent.h"
#include "Model3D.h"

class PropGameComponent : public GameComponent {
private:
    Model3D model;
    Vector3 position;
    std::string modelPath;
    float modelScale;

    float collisionRadius;
    Vector3 collisionCenterOffset;

    ID3D11Buffer* debugVertexBuffer;
    ID3D11Buffer* debugIndexBuffer;
    ID3D11VertexShader* debugVS;
    ID3D11PixelShader* debugPS;
    ID3D11InputLayout* debugLayout;
    ID3D11Buffer* debugConstantBuffer;
    bool debugInitialized;

public:
    PropGameComponent(Game* game, const std::string& path, const Vector3& startPosition = Vector3(0, 0, 0), float scale = 0.15f);
    ~PropGameComponent();

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

    void InitDebugCollider();
    void DrawDebugCollider();
};