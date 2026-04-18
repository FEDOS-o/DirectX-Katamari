// Core.h
#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

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

struct PSConstantBuffer {
    Vector4 cameraPosition;
    Vector4 objectColor;
    int useTexture;
    int hasMaterial;
    int useReflection;      // <-- ÄÎÁÀÂÈÒÜ
    float padding;          // <-- èçìåíèòü ðàçìåð padding
};

struct MaterialBuffer {
    Vector4 ambient;
    Vector4 diffuse;
    Vector4 specular;
    float shininess;
    float padding[3];
};

struct DirectionalLightBuffer {
    Vector4 ambient;
    Vector4 diffuse;
    Vector4 specular;
    Vector3 direction;
    float padding;
};