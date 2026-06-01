#pragma once
#include "LightComponent.h"

class DirectionalLightComponent : public LightComponent {
private:
    Vector3 direction;
    bool castShadows;

public:
    DirectionalLightComponent(Game* game,
        const Vector3& inDirection = Vector3(0.5f, -1.0f, 0.3f),
        const Vector4& inColor = Vector4(1, 1, 1, 1),
        float inIntensity = 1.0f,
        bool inCastShadows = true)
        : LightComponent(game, inColor, inIntensity), castShadows(inCastShadows) {
        SetDirection(inDirection);
    }

    void SetDirection(const Vector3& dir) {
        direction = dir;
        direction.Normalize();
    }

    Vector3 GetDirection() const { return direction; }

    GPULight GetGPUData() const override {
        GPULight data;
        data.position = Vector4(0, 0, 0, 0);
        data.color = Vector4(color.x, color.y, color.z, intensity);
        data.attenuation = Vector4(1, 0, 0, 0);
        data.direction = Vector4(direction.x, direction.y, direction.z, castShadows ? 1.0f : 0.0f);
        return data;
    }

    bool CastsShadows() const override { return castShadows; }
};