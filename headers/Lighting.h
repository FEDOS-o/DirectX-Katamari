#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct Material {
    Vector4 ambient;
    Vector4 diffuse;
    Vector4 specular;
    float shininess;
    float padding[3];

    Material() : ambient(0.2f, 0.2f, 0.2f, 1.0f),
        diffuse(1.0f, 1.0f, 1.0f, 1.0f),
        specular(0.5f, 0.5f, 0.5f, 1.0f),
        shininess(32.0f) {
        padding[0] = padding[1] = padding[2] = 0;
    }

    Material(const Vector4& diff, float shine = 32.0f)
        : ambient(0.2f, 0.2f, 0.2f, 1.0f),
        diffuse(diff),
        specular(0.5f, 0.5f, 0.5f, 1.0f),
        shininess(shine) {
        padding[0] = padding[1] = padding[2] = 0;
    }
};


struct DirectionalLight {
    Vector4 ambient;
    Vector4 diffuse;
    Vector4 specular;
    Vector3 direction;
    float padding;

    DirectionalLight() : ambient(0.15f, 0.15f, 0.15f, 1.0f),
        diffuse(0.8f, 0.8f, 0.8f, 1.0f),
        specular(1.0f, 1.0f, 1.0f, 1.0f),
        direction(0.5f, -1.0f, 0.3f),
        padding(0) {
        direction.Normalize();
    }
};