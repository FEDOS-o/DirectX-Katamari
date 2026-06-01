#pragma once
#include "LightComponent.h"

class PointLightComponent : public LightComponent {
private:
    Vector3 position;
    float range;
    float constantAtt;
    float linearAtt;
    float quadraticAtt;

public:
    PointLightComponent(Game* game,
        const Vector3& inPosition = Vector3(0, 5, 0),
        const Vector4& inColor = Vector4(1, 0.8f, 0.6f, 1),
        float inIntensity = 1.0f,
        float inRange = 20.0f)
        : LightComponent(game, inColor, inIntensity),
        position(inPosition), range(inRange),
        constantAtt(1.0f), linearAtt(0.09f), quadraticAtt(0.032f) {
    }

    Vector3 GetPosition() const { return position; }
    void SetPosition(const Vector3& pos) { position = pos; }

    float GetRange() const { return range; }
    void SetRange(float r) { range = r; }

    void SetAttenuation(float constant, float linear, float quadratic) {
        constantAtt = constant;
        linearAtt = linear;
        quadraticAtt = quadratic;
    }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    GPULight GetGPUData() const override {
        GPULight data;
        data.position = Vector4(position.x, position.y, position.z, 1.0f);
        data.color = Vector4(color.x, color.y, color.z, intensity);
        data.attenuation = Vector4(constantAtt, linearAtt, quadraticAtt, range);
        data.direction = Vector4(0, 0, 0, 0);
        return data;
    }
};