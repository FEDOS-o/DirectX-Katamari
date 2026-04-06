#pragma once
#include "GameComponent.h"
#include "SphereRenderer.h"
#include "OrbitalCameraGameComponent.h"

class CelestialBodyGameComponent : public GameComponent {
protected:
    Vector3 position;
    float size;
    Vector4 color;
    CelestialBodyGameComponent* parent;

    Vector3 selfRotation;
    Vector3 selfRotationAxis;
    float selfRotationSpeed;

    float orbitRadius;
    float orbitSpeed;
    float orbitAngle;
    Vector3 orbitAxis;

    SphereRenderer renderer;
    int segments;

public:
    CelestialBodyGameComponent(Game* game, CelestialBodyGameComponent* parent,
        float size, const Vector4& color,
        float selfRotationSpeed = 0.0f,
        const Vector3& selfRotationAxis = Vector3(0, 1, 0),
        float orbitRadius = 0.0f, float orbitSpeed = 0.0f,
        const Vector3& orbitAxis = Vector3(0, 1, 0),
        int segments = 32) :
        GameComponent(game), parent(parent), position(0, 0, 0), size(size), color(color),
        selfRotation(0, 0, 0), selfRotationSpeed(selfRotationSpeed), orbitRadius(orbitRadius),
        orbitSpeed(orbitSpeed), orbitAngle(0), segments(segments) {

        Vector3 normSelf = selfRotationAxis;
        if (normSelf.Length() > 0.001f) normSelf.Normalize();
        else normSelf = Vector3(0, 1, 0);
        this->selfRotationAxis = normSelf;

        Vector3 normOrbit = orbitAxis;
        if (normOrbit.Length() > 0.001f) normOrbit.Normalize();
        else normOrbit = Vector3(0, 1, 0);
        this->orbitAxis = normOrbit;
    }

    void Initialize() override {
        renderer.Initialize(game, color, segments);
    }

    void Update(float deltaTime) override {
        selfRotation += selfRotationAxis * selfRotationSpeed * deltaTime;

        if (parent && orbitRadius > 0) {
            orbitAngle += orbitSpeed * deltaTime;
            Quaternion quat = Quaternion::CreateFromAxisAngle(orbitAxis, orbitAngle);
            Vector3 orbitPos = Vector3(orbitRadius, 0, 0);
            orbitPos = Vector3::Transform(orbitPos, quat);
            position = parent->GetPosition() + orbitPos;
        }
    }

    void Draw() override {
        if (!game || !game->Camera) {
            return;
        }

        Quaternion rot = Quaternion::CreateFromYawPitchRoll(selfRotation.y, selfRotation.x, selfRotation.z);
        Matrix world = Matrix::CreateScale(size) *
            Matrix::CreateFromQuaternion(rot) *
            Matrix::CreateTranslation(position);

        renderer.Draw(game, world, color,
            game->Camera->GetViewMatrix(),
            game->Camera->GetProjectionMatrix());
    }

    void DestroyResources() override {
        renderer.DestroyResources();
    }

    Vector3 GetPosition() const { 
        return position;
    }
};