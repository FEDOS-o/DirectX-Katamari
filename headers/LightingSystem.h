#pragma once
#include <vector>
#include <algorithm>
#include "LightData.h"
#include "LightComponent.h"
#include "DirectionalLightComponent.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"

class LightingSystem {
private:
    std::vector<LightComponent*> lights;
    DirectionalLightComponent* mainDirectional;

public:
    LightingSystem() : mainDirectional(nullptr) {}

    ~LightingSystem() = default;

    void AddLight(LightComponent* light) {
        if (!light) return;
        lights.push_back(light);

        DirectionalLightComponent* dir = dynamic_cast<DirectionalLightComponent*>(light);
        if (dir && !mainDirectional) {
            mainDirectional = dir;
        }
    }

    void RemoveLight(LightComponent* light) {
        auto it = std::remove(lights.begin(), lights.end(), light);
        lights.erase(it, lights.end());

        if (static_cast<LightComponent*>(mainDirectional) == light) {
            mainDirectional = nullptr;
            for (auto* l : lights) {
                DirectionalLightComponent* dir = dynamic_cast<DirectionalLightComponent*>(l);
                if (dir) {
                    mainDirectional = dir;
                    break;
                }
            }
        }
    }

    void Clear() {
        lights.clear();
        mainDirectional = nullptr;
    }

    DirectionalLightComponent* GetMainDirectional() const { return mainDirectional; }

    int GetLightCount() const { return (int)std::min(lights.size(), size_t(8)); }

    void FillLightBuffer(LightBuffer& buffer) const {
        buffer.lightCount = GetLightCount();
        int count = buffer.lightCount;
        for (int i = 0; i < count; i++) {
            buffer.lights[i] = lights[i]->GetGPUData();
        }
        for (int i = count; i < 8; i++) {
            buffer.lights[i] = GPULight();
            buffer.lights[i].attenuation = Vector4(1, 0, 0, 0);
            buffer.lights[i].color = Vector4(0, 0, 0, 0);
            buffer.lights[i].position = Vector4(0, 0, 0, 0);
            buffer.lights[i].direction = Vector4(0, 0, 0, 0);
        }
    }

    void UpdateLights(float deltaTime) {
        for (auto* light : lights) {
            if (light) light->Update(deltaTime);
        }
    }

    const std::vector<LightComponent*>& GetLights() const { return lights; }
};