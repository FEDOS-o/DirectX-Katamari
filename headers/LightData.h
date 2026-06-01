#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

// Типы источников света
enum LightType : int {
    LIGHT_TYPE_DIRECTIONAL = 0,
    LIGHT_TYPE_POINT = 1,
    LIGHT_TYPE_SPOT = 2
};

// GPU структура для света
struct GPULight {
    Vector4 position;      // xyz позиция, w = тип света (0=dir, 1=point, 2=spot)
    Vector4 color;         // RGB + intensity в w
    Vector4 attenuation;   // x=constant, y=linear, z=quadratic, w=range
    Vector4 direction;     // xyz direction, w = cos(spotAngle) для spot, 0 для других
};

struct LightBuffer {
    GPULight lights[8];
    int lightCount;
    Vector3 padding;
};