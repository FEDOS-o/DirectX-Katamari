#pragma once
#include "LightData.h"

class Game;

class LightComponent {
protected:
    Game* game;
    Vector4 color;
    float intensity;

public:
    LightComponent(Game* inGame, const Vector4& inColor = Vector4(1, 1, 1, 1), float inIntensity = 1.0f)
        : game(inGame), color(inColor), intensity(inIntensity) {
    }

    virtual ~LightComponent() = default;

    Vector4 GetColor() const { return color; }
    void SetColor(const Vector4& c) { color = c; }

    float GetIntensity() const { return intensity; }
    void SetIntensity(float i) { intensity = i; }

    virtual GPULight GetGPUData() const = 0;
    virtual bool CastsShadows() const { return false; }
    virtual void Update(float deltaTime) { (void)deltaTime; }
};