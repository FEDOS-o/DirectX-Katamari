#pragma once
#include "GameComponent.h"
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class Camera : public GameComponent {
protected:
    Matrix viewMatrix;
    Matrix projectionMatrix;
    bool isPerspective;

public:
    Camera(Game* game) : GameComponent(game), isPerspective(true) {}
    virtual ~Camera() {}

    virtual void UpdateCamera() = 0;
    virtual void UpdateProjection() = 0;
    virtual void ResetCamera() = 0;

    Matrix GetViewMatrix() const { return viewMatrix; }
    Matrix GetProjectionMatrix() const { return projectionMatrix; }
    bool IsPerspective() const { return isPerspective; }

    void SetPerspective(bool perspective) {
        isPerspective = perspective;
        UpdateProjection();
    }

    virtual Vector3 GetPosition() const { return Vector3::Zero; }
    virtual Vector3 GetForward() const { return Vector3(0, 0, 1); }
};