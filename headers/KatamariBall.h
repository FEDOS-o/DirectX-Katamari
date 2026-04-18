#pragma once
#include "GameComponent.h"
#include "TextureLoader.h"
#include "Prop.h"
#include "SphereRenderer.h"
#include "OrbitalCamera.h"
#include <SimpleMath.h>
#include <vector>
#include <set>
#include <string>

using namespace DirectX::SimpleMath;

struct AttachedObject {
    Prop* prop = nullptr;
    Vector3 relativePosition = Vector3::Zero;
    float depth = 0.0f;
    Vector3 originalScale = Vector3(1, 1, 1);
    float attachmentTime = 0.0f;

    AttachedObject() = default;
    AttachedObject(Prop* p, const Vector3& relPos, float attachDepth, const Vector3& scale)
        : prop(p), relativePosition(relPos), depth(attachDepth), originalScale(scale) {
    }
};

#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter

class KatamariBall : public GameComponent {
private:
    Vector3 position;
    float radius;
    float targetRadius;
    Vector3 velocity = Vector3::Zero;
    float moveSpeed = 5.0f;
    float rotationAngle = 0.0f;
    Quaternion ballWorldRotation = Quaternion::Identity;
    OrbitalCamera* camera;
    std::vector<AttachedObject> attachedObjects;
    std::set<Prop*> attachedSet;
    float growAnimationTime = 0.0f;

    Render::SphereRenderer sphereRenderer;
    Vector4 ballColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    bool sphereInitialized = false;

    ID3D11ShaderResourceView* ballTexture = nullptr;
    std::string texturePath;

    float attachedMoveAngleH = 0.0f;
    float attachedMoveAngleV = 0.0f;

    // Debug rendering
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
    std::vector<GameComponent*> props;

    KatamariBall(Game* game, OrbitalCamera* cam,
        const Vector3& startPos = Vector3(0, 0.5f, 0),
        float startRadius = 0.6f,
        const std::string& textureFile = "");

    ~KatamariBall();

    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DestroyResources() override;

    void ProcessInput(float deltaTime);
    void ProcessAttachedObjectInput(float deltaTime);
    void CheckCollisions(std::vector<GameComponent*>& props);
    bool AttachProp(Prop* prop);
    void UpdateAttachedObjects(float deltaTime);

    Vector3 GetPosition() const { return position; }
    float GetRadius() const { return radius; }
    int GetAttachedCount() const { return (int)attachedObjects.size(); }

    void DrawBall();
};

#pragma warning(pop)