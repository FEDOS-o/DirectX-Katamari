#pragma once
#include "LightComponent.h"

using namespace DirectX::SimpleMath;

class SpotLightComponent : public LightComponent {
private:
    Vector3 position;
    Vector3 direction;
    float range;
    float spotAngle;        // в радианах
    float spotFalloff;      // exponent для плавного края
    float constantAtt;
    float linearAtt;
    float quadraticAtt;

public:
    SpotLightComponent(Game* game,
        const Vector3& inPosition = Vector3(0, 5, 0),
        const Vector3& inDirection = Vector3(0, -1, 0),
        const Vector4& inColor = Vector4(1, 0.9f, 0.7f, 1),
        float inIntensity = 1.0f,
        float inRange = 15.0f,
        float inSpotAngle = 3.14 / 3.0f,
        float inSpotFalloff = 2.0f)
        : LightComponent(game, inColor, inIntensity),
        position(inPosition), range(inRange),
        spotAngle(inSpotAngle), spotFalloff(inSpotFalloff),
        constantAtt(1.0f), linearAtt(0.09f), quadraticAtt(0.032f) {
        SetDirection(inDirection);
    }

    void SetPosition(const Vector3& pos) { position = pos; }
    Vector3 GetPosition() const { return position; }

    void SetDirection(const Vector3& dir) {
        direction = dir;
        direction.Normalize();
    }
    Vector3 GetDirection() const { return direction; }

    float GetRange() const { return range; }
    void SetRange(float r) { range = r; }

    float GetSpotAngle() const { return spotAngle; }
    void SetSpotAngle(float angle) { spotAngle = angle; }

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
        // direction.xyz = направление, direction.w = cos(spotAngle)
        data.direction = Vector4(direction.x, direction.y, direction.z, cos(spotAngle));
        return data;
    }
};