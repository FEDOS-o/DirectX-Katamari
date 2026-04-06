#pragma once
#include "Camera.h"
#include "InputDevice.h"
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class FirstPersonCameraGameComponent : public Camera {
private:
    Vector3 position;
    Vector3 forward;
    Vector3 right;
    Vector3 up;
    Vector3 worldUp;

    float yaw;
    float pitch;

    float movementSpeed;
    float mouseSensitivity;

    float minPitch;
    float maxPitch;

    DelegateHandle mouseMoveHandle;

    bool wPressed, sPressed, aPressed, dPressed;
    bool qPressed, ePressed;
    bool shiftPressed;

    bool pPressed;
    bool homePressed;

public:
    FirstPersonCameraGameComponent(Game* game,
        const Vector3& startPosition = Vector3(0, 5, 10),
        float startYaw = -XM_PIDIV4,
        float startPitch = 0.0f)
        : Camera(game)
        , position(startPosition)
        , yaw(startYaw)
        , pitch(startPitch)
        , movementSpeed(10.0f)
        , mouseSensitivity(0.005f)
        , minPitch(-XM_PIDIV2 + 0.01f)
        , maxPitch(XM_PIDIV2 - 0.01f)
        , wPressed(false)
        , sPressed(false)
        , aPressed(false)
        , dPressed(false)
        , qPressed(false)
        , ePressed(false)
        , shiftPressed(false)
        , pPressed(false)
        , homePressed(false)
    {
        worldUp = Vector3(0, 1, 0);
        UpdateViewMatrix();
    }

    void Initialize() override {
        if (!game || !game->Input) {
            return;
        }

        mouseMoveHandle = game->Input->MouseMove.AddLambda([this](const InputDevice::MouseMoveEventArgs& args) {
            yaw -= args.Offset.x * mouseSensitivity;
            pitch -= args.Offset.y * mouseSensitivity;

            if (pitch < minPitch) pitch = minPitch;
            if (pitch > maxPitch) pitch = maxPitch;
            UpdateViewMatrix();
            });
    }

    void Update(float deltaTime) override {
        if (!game || !game->Input) {
            return;
        }

        int wheel = game->Input->MouseWheelDelta;
        if (wheel != 0) {
            movementSpeed += wheel * deltaTime * 5.0f;
            if (movementSpeed < 2.0f) movementSpeed = 2.0f;
            if (movementSpeed > 50.0f) movementSpeed = 50.0f;
        }
        game->Input->MouseWheelDelta = 0;

        wPressed = game->Input->IsKeyDown(Keys::W);
        sPressed = game->Input->IsKeyDown(Keys::S);
        aPressed = game->Input->IsKeyDown(Keys::A);
        dPressed = game->Input->IsKeyDown(Keys::D);
        qPressed = game->Input->IsKeyDown(Keys::Q);
        ePressed = game->Input->IsKeyDown(Keys::E);
        shiftPressed = game->Input->IsKeyDown(Keys::LeftShift);

        ProcessKeyboard(deltaTime);

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
        UpdateViewMatrix();
    }

    void Draw() override {}

    void DestroyResources() override {
        if (game && game->Input) {
            game->Input->MouseMove.Remove(mouseMoveHandle);
        }
    }

    void ProcessKeyboard(float deltaTime) {
        float currentSpeed = movementSpeed * deltaTime;
        if (shiftPressed) {
            currentSpeed *= 2.5f;
        }

        Vector3 movement = Vector3::Zero;

        if (wPressed) movement += forward;
        if (sPressed) movement -= forward;
        if (dPressed) movement += right;
        if (aPressed) movement -= right;
        if (ePressed) movement += worldUp;
        if (qPressed) movement -= worldUp;

        if (movement.LengthSquared() > 0.0f) {
            movement.Normalize();
            position += movement * currentSpeed;
        }
    }

    void UpdateViewMatrix() {
        forward.x = cos(pitch) * sin(yaw);
        forward.y = sin(pitch);
        forward.z = cos(pitch) * cos(yaw);
        forward.Normalize();

        right = forward.Cross(worldUp);
        right.Normalize();

        up = right.Cross(forward);
        up.Normalize();

        viewMatrix = Matrix::CreateLookAt(position, position + forward, up);

        UpdateProjection();
    }

    void UpdateProjection() override {
        float aspect = 800.0f / 800.0f;

        if (isPerspective) {
            projectionMatrix = Matrix::CreatePerspectiveFieldOfView(
                XM_PIDIV4, aspect, 0.1f, 1000.0f);
        }
        else {
            float orthoSize = 20.0f;
            projectionMatrix = Matrix::CreateOrthographic(
                orthoSize * aspect, orthoSize, 0.1f, 1000.0f);
        }
    }

    void UpdateCamera() override {
        UpdateViewMatrix();
    }

    void ResetCamera() override {
        position = Vector3(0, 5, 10);
        yaw = -XM_PIDIV4;
        pitch = 0.0f;
        movementSpeed = 10.0f;
        UpdateViewMatrix();
    }

    void SetPosition(const Vector3& pos) {
        position = pos;
        UpdateViewMatrix();
    }

    Vector3 GetPosition() const override { return position; }
    Vector3 GetForward() const override { return forward; }
    Vector3 GetRight() const { return right; }
    Vector3 GetUp() const { return up; }

    void SetRotation(float newYaw, float newPitch) {
        yaw = newYaw;
        pitch = newPitch;
        if (pitch < minPitch) pitch = minPitch;
        if (pitch > maxPitch) pitch = maxPitch;
        UpdateViewMatrix();
    }

    void SetMovementSpeed(float speed) {
        movementSpeed = speed;
    }

    void SetMouseSensitivity(float sensitivity) {
        mouseSensitivity = sensitivity;
    }

    ~FirstPersonCameraGameComponent() {
        DestroyResources();
    }
};