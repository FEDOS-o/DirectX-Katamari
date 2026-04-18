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

struct PSConstantBuffer {
    Vector4 cameraPosition = Vector4(0, 0, 0, 1);
    Vector4 objectColor = Vector4(1, 1, 1, 1);
    int useTexture = 0;
    int hasMaterial = 0;
    int useReflection = 0;
    float padding = 0.0f;
};

struct MaterialBuffer {
    Vector4 ambient = Vector4(0.2f, 0.2f, 0.2f, 1.0f);
    Vector4 diffuse = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    Vector4 specular = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
    float shininess = 32.0f;
    float padding[3] = { 0.0f, 0.0f, 0.0f };
};

struct DirectionalLightBuffer {
    Vector4 ambient = Vector4(0.15f, 0.15f, 0.15f, 1.0f);
    Vector4 diffuse = Vector4(0.8f, 0.8f, 0.8f, 1.0f);
    Vector4 specular = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    Vector3 direction = Vector3(0.5f, -1.0f, 0.3f);
    float padding = 0.0f;
};

#pragma warning(pop)