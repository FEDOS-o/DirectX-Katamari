#pragma once
#include "GameComponent.h"
#include "Camera.h"
#include "InputDevice.h"
#include "PropGameComponent.h"
#include "SphereRenderer.h"
#include <SimpleMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <set>

using namespace DirectX::SimpleMath;

struct AttachedObject {
    PropGameComponent* prop;
    Vector3 relativePosition;
    Vector3 originalScale;
    float attachmentTime;

    AttachedObject() : prop(nullptr), relativePosition(Vector3::Zero), originalScale(Vector3(1, 1, 1)), attachmentTime(0) {}

    AttachedObject(PropGameComponent* p, const Vector3& relPos, const Vector3& scale)
        : prop(p), relativePosition(relPos), originalScale(scale), attachmentTime(0) {
    }
};

class OrbitalCameraGameComponent;

class KatamariBallGameComponent : public GameComponent {
private:
    Vector3 position;
    float radius;
    float targetRadius;
    Vector3 velocity;
    float moveSpeed;
    float rotationAngle;
    OrbitalCameraGameComponent* camera;
    std::vector<AttachedObject> attachedObjects;
    std::set<PropGameComponent*> attachedSet;
    float growAnimationTime;

    SphereRenderer sphereRenderer;
    Vector4 ballColor;
    bool sphereInitialized;

    // Debug collider
    ID3D11Buffer* debugVertexBuffer;
    ID3D11Buffer* debugIndexBuffer;
    ID3D11VertexShader* debugVS;
    ID3D11PixelShader* debugPS;
    ID3D11InputLayout* debugLayout;
    ID3D11Buffer* debugConstantBuffer;
    bool debugInitialized;

public:
    KatamariBallGameComponent(Game* game, OrbitalCameraGameComponent* cam,
        const Vector3& startPos = Vector3(0, 0, 0),
        float startRadius = 0.6f);
    ~KatamariBallGameComponent();

    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DestroyResources() override;

    void ProcessInput(float deltaTime);
    void CheckCollisions(std::vector<GameComponent*>& props);
    bool AttachProp(PropGameComponent* prop);
    void UpdateAttachedObjects(float deltaTime);

    Vector3 GetPosition() const { return position; }
    float GetRadius() const { return radius; }
    int GetAttachedCount() const { return (int)attachedObjects.size(); }

    void DrawBall();
    void InitDebugCollider();
    void DrawDebugCollider();
};