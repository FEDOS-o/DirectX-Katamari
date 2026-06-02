// Particle.h
#pragma once
#include "GameComponent.h"
#include "RenderingSystem.h"
#include <vector>
#include <random>

using namespace DirectX::SimpleMath;

struct Particle {
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    float life;
    float maxLife;
    float size;
    Vector4 color;

    Particle() : position(Vector3::Zero), velocity(Vector3::Zero),
        acceleration(Vector3::Zero), life(0), maxLife(0),
        size(0.1f), color(Vector4(1, 1, 1, 1)) {
    }
};

class ParticleEmitter : public GameComponent {
private:
    struct ParticleVertex {
        Vector3 position;
        Vector4 color;
    };

    std::vector<Particle> particles;
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;

    Vector3 emitterPosition;
    Vector3 emitterDirection;
    float emissionRate;
    float particlesPerSecond;
    float timeSinceLastEmission;
    int maxParticles;

    float particleSpeedMin;
    float particleSpeedMax;
    float particleLifeMin;
    float particleLifeMax;
    float particleSizeMin;
    float particleSizeMax;

    Vector4 startColor;
    Vector4 endColor;

    Vector3 gravity;
    float groundY;
    float bounceDamping;
    bool useGravity;

    // GPU resources for rendering
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    ID3D11InputLayout* inputLayout;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11Buffer* vsConstantBuffer;
    ID3D11BlendState* additiveBlendState;
    ID3D11DepthStencilState* depthState;

    int currentParticleCount;
    bool initialized;

    // Physics parameters for G-Buffer collision
    float restitution;
    float friction;
    bool useGBufferCollision;

    ID3DBlob* CompileShader(const char* code, const char* target, const char* entry);
    void CreateGeometryBuffers();
    void CreateShaders();
    void CreateStates();
    void UpdateVertexBuffer();
    void EmitParticle();
    void UpdateParticlesWithGBuffer(float deltaTime);
    void UpdateParticlesSimple(float deltaTime);

public:
    ParticleEmitter(Game* game, const Vector3& position);
    ~ParticleEmitter();

    void Initialize() override;
    void Update(float deltaTime) override;
    void Draw() override;
    void DrawGeometry(RenderingSystem* rs) override;
    void DrawShadow() override;
    void DestroyResources() override;

    // Configuration
    void SetEmissionRate(float rate) { particlesPerSecond = rate; }
    void SetMaxParticles(int max) { maxParticles = max; }
    void SetSpeedRange(float minSpeed, float maxSpeed);
    void SetLifeRange(float minLife, float maxLife);
    void SetSizeRange(float minSize, float maxSize);
    void SetColors(const Vector4& start, const Vector4& end);
    void SetDirection(const Vector3& dir);
    void SetGravity(const Vector3& grav);
    void SetGroundCollision(float y, float damping);
    void SetRestitution(float r) { restitution = r; }
    void SetFriction(float f) { friction = f; }
    void UseGBufferCollision(bool use) { useGBufferCollision = use; }

    // Quick setup
    void SetupFountain(const Vector3& pos, const Vector4& color);
};