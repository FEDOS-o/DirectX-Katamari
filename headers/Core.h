// Core.h
#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

#pragma warning(push)
#pragma warning(disable: 26495)

struct Vertex {
    Vector3 position;
    Vector4 color;
    Vector2 texCoord;
    Vector3 normal;
};

struct VSConstantBuffer {
    Matrix world;
    Matrix view;
    Matrix projection;
    Matrix worldInvTranspose;
};

// УНИФИЦИРОВАННАЯ структура PSConstantBuffer для ВСЕХ шейдеров
#pragma pack(push, 16)
struct PSConstantBuffer {
    DirectX::XMFLOAT4 cameraPosition;
    DirectX::XMFLOAT4 objectColor;
    int useTexture;
    int hasMaterial;
    int useReflection;
    int useShadow;
    float shadowBias;
    float padding;
};
#pragma pack(pop)

struct MaterialBuffer {
    Vector4 ambient;
    Vector4 diffuse;
    Vector4 specular;
    float shininess;
    float materialPadding[3];
};

struct DirectionalLightBuffer {
    Vector4 ambient;
    Vector4 diffuse;
    Vector4 specular;
    Vector3 direction;
    float padding;
};

// Основной ShadowConstantBuffer с отдельными матрицами
#pragma pack(push, 16)
struct ShadowConstantBuffer {
    Matrix lightView;
    Matrix lightProjection;
};
#pragma pack(pop)

// Версия с комбинированной матрицей (используется в Game.cpp)
#pragma pack(push, 16)
struct ShadowConstantBufferCombined {
    Matrix lightViewProj;
};
#pragma pack(pop)

struct ShadowWorldConstantBuffer {
    Matrix world;
};

#pragma warning(pop)