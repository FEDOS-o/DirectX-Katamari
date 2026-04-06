#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct Vertex {
    Vector3 position;
    Vector4 color;
    Vector2 texCoord;
};

struct ConstantBuffer {
    Matrix worldViewProj;
    Vector4 objectColor;
};