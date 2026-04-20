#pragma once
#include "Camera.h"
#include "InputDevice.h"
#include <SimpleMath.h>
#include <algorithm>

using namespace DirectX::SimpleMath;
using namespace DirectX;

#pragma warning(push)
#pragma warning(disable: 4100) // unreferenced formal parameter

class OrbitalCamera : public Camera {
private:
    Vector3 target;
    float distance;
    float yaw;
    float pitch;
    float minDistance = 3.0f;
    float maxDistance = 80.0f;
    float minPitch = -1.4f;
    float maxPitch = 1.4f;
    float rotationSpeed = 0.005f;
    float zoomSpeed = 2.0f;
    DelegateHandle mouseHandle;
    bool pPressed = false;
    bool homePressed = false;

public:
    OrbitalCamera(Game* game, const Vector3& target = Vector3(0, 2, 0),
        float distance = 25.0f, float yaw = 0.0f, float pitch = 0.7f) :
        Camera(game), target(target), distance(distance), yaw(yaw), pitch(pitch) {
        UpdateCamera();
    }

    void Initialize() override {
        if (!game || !game->Input) return;

        mouseHandle = game->Input->MouseMove.AddLambda([this](const InputDevice::MouseMoveEventArgs& args) {
            yaw -= args.Offset.x * rotationSpeed;
            pitch += args.Offset.y * rotationSpeed;
            pitch = std::clamp(pitch, minPitch, maxPitch);
            UpdateCamera();
            });
    }

    void Update(float deltaTime) override {
        if (!game || !game->Input) return;

        int wheel = game->Input->MouseWheelDelta;
        if (wheel != 0) {
            distance -= wheel * zoomSpeed * deltaTime;
            distance = std::clamp(distance, minDistance, maxDistance);
            UpdateCamera();
        }
        game->Input->MouseWheelDelta = 0;

        if (game->Input->IsKeyDown(Keys::P)) {
            if (!pPressed) {
                isPerspective = !isPerspective;
                UpdateProjection();
                pPressed = true;
            }
        }
        else {
            pPressed = false;
        }

        if (game->Input->IsKeyDown(Keys::Home)) {
            if (!homePressed) {
                ResetCamera();
                homePressed = true;
            }
        }
        else {
            homePressed = false;
        }
    }

    void UpdateCamera() override {
        float x = distance * cos(pitch) * sin(yaw);
        float y = distance * sin(pitch);
        float z = distance * cos(pitch) * cos(yaw);
        viewMatrix = Matrix::CreateLookAt(target + Vector3(x, y, z), target, Vector3(0, 1, 0));
        UpdateProjection();
    }

    void UpdateProjection() override {
        float aspect = 800.0f / 800.0f;
        if (isPerspective) {
            projectionMatrix = Matrix::CreatePerspectiveFieldOfView(XM_PIDIV4, aspect, 0.5f, 1000.0f);
        }
        else {
            float orthoSize = 80.0f;
            projectionMatrix = Matrix::CreateOrthographic(orthoSize * aspect, orthoSize, 0.1f, 1000.0f);
        }
    }

    void ResetCamera() override {
        target = Vector3(0, 2, 0);
        distance = 25.0f;
        yaw = 0.0f;
        pitch = 0.7f;
        UpdateCamera();
    }

    void SetTarget(const Vector3& t) {
        target = t;
        UpdateCamera();
    }

    Vector3 GetPosition() const override {
        float x = distance * cos(pitch) * sin(yaw);
        float y = distance * sin(pitch);
        float z = distance * cos(pitch) * cos(yaw);
        return target + Vector3(x, y, z);
    }

    Vector3 GetForward() const override {
        Vector3 pos = GetPosition();
        pos = (target - pos);
        pos.Normalize();
        return pos;
    }

    Vector3 GetRight() const {
        Vector3 forward = GetForward();
        Vector3 up(0, 1, 0);
        Vector3 right = forward.Cross(up);
        right.Normalize();
        return right;
    }

    ~OrbitalCamera() {
        if (game && game->Input) {
            game->Input->MouseMove.Remove(mouseHandle);
        }
    }
};

#pragma warning(pop)