#include "KatamariBallGameComponent.h"
#include "OrbitalCameraGameComponent.h"
#include "PropGameComponent.h"
#include "Game.h"
#include <d3dcompiler.h>
#include <iostream>
#include <cmath>
#include <wincodec.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "windowscodecs.lib")

KatamariBallGameComponent::KatamariBallGameComponent(Game* game, OrbitalCameraGameComponent* cam,
    const Vector3& startPos, float startRadius, const std::string& textureFile)
    : GameComponent(game)
    , position(startPos)
    , radius(startRadius)
    , targetRadius(startRadius)
    , velocity(Vector3::Zero)
    , moveSpeed(5.0f)
    , rotationAngle(0)
    , ballWorldRotation(Quaternion::Identity)
    , camera(cam)
    , growAnimationTime(0)
    , ballColor(1.0f, 1.0f, 1.0f, 1.0f)
    , sphereInitialized(false)
    , debugVertexBuffer(nullptr)
    , debugIndexBuffer(nullptr)
    , debugVS(nullptr)
    , debugPS(nullptr)
    , debugLayout(nullptr)
    , debugConstantBuffer(nullptr)
    , debugInitialized(false)
    , ballTexture(nullptr)
    , textureLoaded(false)
    , texturePath(textureFile)
{
}

KatamariBallGameComponent::~KatamariBallGameComponent() {
    DestroyResources();
}

void KatamariBallGameComponent::LoadTexture(const std::string& filepath) {
    if (!game || !game->Device) return;
    if (textureLoaded) return;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

    if (SUCCEEDED(hr)) {
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        std::wstring wpath(filepath.begin(), filepath.end());
        hr = wicFactory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

        if (SUCCEEDED(hr)) {
            Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
            hr = decoder->GetFrame(0, &frame);

            if (SUCCEEDED(hr)) {
                Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
                hr = wicFactory->CreateFormatConverter(&converter);

                if (SUCCEEDED(hr)) {
                    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);

                    if (SUCCEEDED(hr)) {
                        UINT width, height;
                        converter->GetSize(&width, &height);

                        std::vector<BYTE> pixels(width * height * 4);
                        hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());

                        if (SUCCEEDED(hr)) {
                            D3D11_TEXTURE2D_DESC texDesc = {};
                            texDesc.Width = width;
                            texDesc.Height = height;
                            texDesc.MipLevels = 1;
                            texDesc.ArraySize = 1;
                            texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                            texDesc.SampleDesc.Count = 1;
                            texDesc.SampleDesc.Quality = 0;
                            texDesc.Usage = D3D11_USAGE_DEFAULT;
                            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                            texDesc.CPUAccessFlags = 0;
                            texDesc.MiscFlags = 0;

                            D3D11_SUBRESOURCE_DATA initData = {};
                            initData.pSysMem = pixels.data();
                            initData.SysMemPitch = width * 4;

                            ID3D11Texture2D* tex = nullptr;
                            hr = game->Device->CreateTexture2D(&texDesc, &initData, &tex);

                            if (SUCCEEDED(hr) && tex) {
                                hr = game->Device->CreateShaderResourceView(tex, nullptr, &ballTexture);
                                tex->Release();

                                if (SUCCEEDED(hr) && ballTexture) {
                                    textureLoaded = true;
                                    std::cout << "[Ball] Texture loaded: " << filepath << std::endl;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    CoUninitialize();
}

void KatamariBallGameComponent::Initialize() {
    if (!sphereInitialized && game && game->Device) {
        sphereRenderer.Initialize(game, ballColor, 48);
        sphereInitialized = true;
        InitDebugCollider();

        if (!texturePath.empty()) {
            LoadTexture(texturePath);
        }

        std::cout << "[Ball] Initialized! Radius: " << radius << std::endl;
    }
}

void KatamariBallGameComponent::InitDebugCollider() {
    if (debugInitialized || !game || !game->Device) return;

    const char* vsCode = R"(
        cbuffer ConstantBuffer : register(b0) {
            float4x4 viewProj;
        };
        
        struct VSInput {
            float3 position : POSITION;
            float4 color : COLOR;
        };
        
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };
        
        VSOutput VSMain(VSInput input) {
            VSOutput output;
            output.position = mul(float4(input.position, 1.0f), viewProj);
            output.color = input.color;
            return output;
        }
    )";

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* error = nullptr;

    HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
        "VSMain", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, &error);

    if (FAILED(hr)) {
        if (error) error->Release();
        return;
    }

    const char* psCode = R"(
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };
        
        float4 PSMain(VSOutput input) : SV_TARGET {
            return input.color;
        }
    )";

    ID3DBlob* psBlob = nullptr;
    hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr,
        "PSMain", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, &error);

    if (FAILED(hr)) {
        if (error) error->Release();
        vsBlob->Release();
        return;
    }

    game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &debugVS);
    game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &debugPS);

    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    game->Device->CreateInputLayout(elements, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &debugLayout);

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(Matrix);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    game->Device->CreateBuffer(&cbDesc, nullptr, &debugConstantBuffer);

    vsBlob->Release();
    psBlob->Release();

    debugInitialized = true;
}

void KatamariBallGameComponent::DrawDebugCollider() {
    if (!debugInitialized || !game || !game->Context || !game->Camera) return;

    struct DebugVertex { Vector3 position; Vector4 color; };
    std::vector<DebugVertex> vertices;
    std::vector<UINT> indices;

    int segments = 24;
    float angleStep = 2.0f * 3.14159f / segments;

    // XZ plane (horizontal) - зеленая сфера коллайдера
    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float x = cos(angle) * radius;
        float z = sin(angle) * radius;
        vertices.push_back({ Vector3(position.x + x, position.y, position.z + z), Vector4(0, 1, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    int xzStart = (int)vertices.size() - (segments + 1);

    // XY plane (vertical)
    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float x = cos(angle) * radius;
        float y = sin(angle) * radius;
        vertices.push_back({ Vector3(position.x + x, position.y + y, position.z), Vector4(0, 1, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(xzStart + segments + 1 + i);
        indices.push_back(xzStart + segments + 1 + i + 1);
    }

    int xyStart = xzStart + segments + 1;

    // YZ plane
    for (int i = 0; i <= segments; i++) {
        float angle = i * angleStep;
        float y = cos(angle) * radius;
        float z = sin(angle) * radius;
        vertices.push_back({ Vector3(position.x, position.y + y, position.z + z), Vector4(0, 1, 0, 1) });
    }
    for (int i = 0; i < segments; i++) {
        indices.push_back(xyStart + segments + 1 + i);
        indices.push_back(xyStart + segments + 1 + i + 1);
    }

    // ===== ВИЗУАЛИЗАЦИЯ ОСИ ВРАЩЕНИЯ ШАРА =====
    float speed = velocity.Length();
    if (speed > 0.01f) {
        Vector3 moveDir = velocity;
        moveDir.y = 0;
        if (moveDir.Length() > 0.01f) {
            moveDir.Normalize();

            // Ось вращения: перпендикулярна moveDir и UP (горизонтальная)
            Vector3 rotAxis = Vector3(0, 1, 0).Cross(moveDir);
            if (rotAxis.Length() < 0.1f) {
                rotAxis = Vector3(1, 0, 0);
            }
            else {
                rotAxis.Normalize();
            }

            // Рисуем ось вращения (красная линия)
            Vector3 axisStart = position - rotAxis * radius * 1.5f;
            Vector3 axisEnd = position + rotAxis * radius * 1.5f;

            Vector3 axisStep = (axisEnd - axisStart) / 10.0f;
            for (int i = 0; i < 10; i++) {
                Vector3 p1 = axisStart + axisStep * i;
                Vector3 p2 = axisStart + axisStep * (i + 1);
                vertices.push_back({ p1, Vector4(1, 0, 0, 1) });
                vertices.push_back({ p2, Vector4(1, 0, 0, 1) });
                indices.push_back(vertices.size() - 2);
                indices.push_back(vertices.size() - 1);
            }

            // Рисуем направление движения (синяя линия со стрелкой)
            Vector3 moveStart = position;
            Vector3 moveEnd = position + moveDir * radius * 1.5f;
            Vector3 moveStep = (moveEnd - moveStart) / 10.0f;

            for (int i = 0; i < 10; i++) {
                Vector3 p1 = moveStart + moveStep * i;
                Vector3 p2 = moveStart + moveStep * (i + 1);
                vertices.push_back({ p1, Vector4(0, 0, 1, 1) });
                vertices.push_back({ p2, Vector4(0, 0, 1, 1) });
                indices.push_back(vertices.size() - 2);
                indices.push_back(vertices.size() - 1);
            }

            // Стрелка
            Vector3 arrowBase = moveEnd - moveStep;
            Vector3 arrowRight = rotAxis * 0.2f;
            Vector3 arrowUp = Vector3(0, 1, 0).Cross(moveDir) * 0.2f;

            vertices.push_back({ moveEnd, Vector4(0, 0, 1, 1) });
            vertices.push_back({ arrowBase + arrowRight, Vector4(0, 0, 1, 1) });
            vertices.push_back({ arrowBase - arrowRight, Vector4(0, 0, 1, 1) });
            vertices.push_back({ arrowBase + arrowUp, Vector4(0, 0, 1, 1) });
            vertices.push_back({ arrowBase - arrowUp, Vector4(0, 0, 1, 1) });

            int arrowStart = (int)vertices.size() - 5;
            indices.push_back(arrowStart);
            indices.push_back(arrowStart + 1);
            indices.push_back(arrowStart);
            indices.push_back(arrowStart + 2);
            indices.push_back(arrowStart);
            indices.push_back(arrowStart + 3);
            indices.push_back(arrowStart);
            indices.push_back(arrowStart + 4);
        }
    }
    else {
        // Если стоим, рисуем оси X и Z для ориентации
        Vector3 xAxis = Vector3(1, 0, 0);
        Vector3 zAxis = Vector3(0, 0, 1);

        Vector3 xStart = position - xAxis * radius * 1.5f;
        Vector3 xEnd = position + xAxis * radius * 1.5f;
        for (int i = 0; i < 10; i++) {
            Vector3 p1 = xStart + (xEnd - xStart) * (i / 10.0f);
            Vector3 p2 = xStart + (xEnd - xStart) * ((i + 1) / 10.0f);
            vertices.push_back({ p1, Vector4(1, 0, 0, 0.5f) });
            vertices.push_back({ p2, Vector4(1, 0, 0, 0.5f) });
            indices.push_back(vertices.size() - 2);
            indices.push_back(vertices.size() - 1);
        }

        Vector3 zStart = position - zAxis * radius * 1.5f;
        Vector3 zEnd = position + zAxis * radius * 1.5f;
        for (int i = 0; i < 10; i++) {
            Vector3 p1 = zStart + (zEnd - zStart) * (i / 10.0f);
            Vector3 p2 = zStart + (zEnd - zStart) * ((i + 1) / 10.0f);
            vertices.push_back({ p1, Vector4(0, 0, 1, 0.5f) });
            vertices.push_back({ p2, Vector4(0, 0, 1, 0.5f) });
            indices.push_back(vertices.size() - 2);
            indices.push_back(vertices.size() - 1);
        }
    }

    // Update vertex buffer
    if (debugVertexBuffer) { debugVertexBuffer->Release(); debugVertexBuffer = nullptr; }
    if (debugIndexBuffer) { debugIndexBuffer->Release(); debugIndexBuffer = nullptr; }

    D3D11_BUFFER_DESC vertexDesc = {};
    vertexDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexDesc.ByteWidth = sizeof(DebugVertex) * (UINT)vertices.size();
    vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = { vertices.data() };
    game->Device->CreateBuffer(&vertexDesc, &vertexData, &debugVertexBuffer);

    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_DEFAULT;
    indexDesc.ByteWidth = sizeof(UINT) * (UINT)indices.size();
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = { indices.data() };
    game->Device->CreateBuffer(&indexDesc, &indexData, &debugIndexBuffer);

    Matrix viewProj = game->Camera->GetViewMatrix() * game->Camera->GetProjectionMatrix();
    Matrix transposed = viewProj.Transpose();
    game->Context->UpdateSubresource(debugConstantBuffer, 0, nullptr, &transposed, 0, 0);

    UINT stride = sizeof(DebugVertex);
    UINT offset = 0;
    game->Context->IASetVertexBuffers(0, 1, &debugVertexBuffer, &stride, &offset);
    game->Context->IASetIndexBuffer(debugIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    game->Context->IASetInputLayout(debugLayout);
    game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    game->Context->VSSetShader(debugVS, nullptr, 0);
    game->Context->VSSetConstantBuffers(0, 1, &debugConstantBuffer);
    game->Context->PSSetShader(debugPS, nullptr, 0);

    game->Context->DrawIndexed((UINT)indices.size(), 0, 0);
}

void KatamariBallGameComponent::Update(float deltaTime) {
    ProcessInput(deltaTime);

    position += velocity * deltaTime;

    if (position.y < radius) {
        position.y = radius;
    }

    if (camera) {
        camera->SetTarget(position);
    }

    // Вращение шара в направлении движения
    float speed = velocity.Length();
    if (speed > 0.01f) {
        float distance = speed * deltaTime;
        float circumference = 2.0f * 3.14159f * radius;
        float angleDelta = (distance / circumference) * 2.0f * 3.14159f;

        Vector3 moveDir = velocity;
        moveDir.y = 0;
        if (moveDir.Length() > 0.01f) {
            moveDir.Normalize();

            // Ось вращения (та же самая, что рисуется в дебаге)
            Vector3 rotAxis = Vector3(0, 1, 0).Cross(moveDir);
            if (rotAxis.Length() < 0.1f) {
                rotAxis = Vector3(1, 0, 0);
            }
            else {
                rotAxis.Normalize();
            }

            // ПРОСТО накапливаем угол
            rotationAngle += angleDelta;
            while (rotationAngle > XM_2PI) rotationAngle -= XM_2PI;
        }
    }

    if (growAnimationTime > 0) {
        growAnimationTime -= deltaTime;
        float t = 1.0f - (growAnimationTime / 0.3f);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        radius = radius * (1 - t) + targetRadius * t;

        if (growAnimationTime <= 0) {
            radius = targetRadius;
            std::cout << "[Ball] Growth complete! New radius: " << radius << std::endl;
        }
    }

    UpdateAttachedObjects(deltaTime);
    CheckCollisions(props);
}

void KatamariBallGameComponent::ProcessInput(float deltaTime) {
    if (!game || !game->Input) return;

    Vector3 inputDir = Vector3::Zero;

    if (game->Input->IsKeyDown(Keys::W)) inputDir.z += 1;
    if (game->Input->IsKeyDown(Keys::S)) inputDir.z -= 1;
    if (game->Input->IsKeyDown(Keys::D)) inputDir.x += 1;
    if (game->Input->IsKeyDown(Keys::A)) inputDir.x -= 1;

    if (inputDir.LengthSquared() > 0) {
        inputDir.Normalize();

        if (camera) {
            Vector3 forward = camera->GetForward();
            Vector3 up(0, 1, 0);
            Vector3 right = forward.Cross(up);
            right.Normalize();
            forward = up.Cross(right);
            forward.Normalize();

            forward.y = 0;
            right.y = 0;
            forward.Normalize();
            right.Normalize();

            velocity = (forward * inputDir.z + right * inputDir.x) * moveSpeed;
        }
        else {
            velocity = inputDir * moveSpeed;
        }
    }
    else {
        velocity *= 0.95f;
    }
}

void KatamariBallGameComponent::CheckCollisions(std::vector<GameComponent*>& props) {
    for (auto* comp : props) {
        PropGameComponent* prop = dynamic_cast<PropGameComponent*>(comp);
        if (!prop) continue;
        if (attachedSet.find(prop) != attachedSet.end()) continue;

        Vector3 ballCenter = position;
        Vector3 propCenter = prop->GetCollisionCenter();
        float propRadius = prop->GetCollisionRadius();

        float dx = ballCenter.x - propCenter.x;
        float dy = ballCenter.y - propCenter.y;
        float dz = ballCenter.z - propCenter.z;
        float distanceSquared = dx * dx + dy * dy + dz * dz;
        float collisionDistance = radius + propRadius;
        float collisionDistanceSquared = collisionDistance * collisionDistance;

        if (distanceSquared < collisionDistanceSquared) {
            AttachProp(prop);
        }
    }
}

bool KatamariBallGameComponent::AttachProp(PropGameComponent* prop) {
    if (!prop) return false;
    if (attachedSet.find(prop) != attachedSet.end()) return false;

    Vector3 propPos = prop->GetPosition();
    Vector3 direction = propPos - position;

    if (direction.Length() > 0.001f) {
        direction.Normalize();
    }
    else {
        direction = Vector3(1, 0, 0);
    }

    // Сохраняем ЛОКАЛЬНОЕ направление на сфере
    Vector3 localDirection = direction;

    // Глубина прикрепления (абсолютная)
    float propRadius = prop->GetCollisionRadius();
    float absoluteDepth = propRadius * 0.3f;

    // Преобразуем в ОТНОСИТЕЛЬНУЮ глубину (от 0 до 1 относительно текущего радиуса)
    float relativeDepth = absoluteDepth / radius;

    // Если объект уже касается шара
    float distanceToSurface = (propPos - position).Length() - radius;
    if (distanceToSurface < 0) {
        absoluteDepth = -distanceToSurface + propRadius * 0.2f;
        relativeDepth = absoluteDepth / radius;
    }

    Vector3 surfacePos = position + direction * (radius - absoluteDepth);
    Vector3 originalScale = prop->GetModel().GetScale();

    attachedObjects.push_back(AttachedObject(prop, localDirection, relativeDepth, originalScale));
    attachedSet.insert(prop);

    prop->SetPosition(surfacePos);
    prop->GetModel().SetRotation(Vector3(0, 0, 0));

    float avgScale = (originalScale.x + originalScale.y + originalScale.z) / 3.0f;
    float radiusIncrease = 0.05f + avgScale * 0.1f;
    float newRadius = radius + radiusIncrease;

    targetRadius = newRadius;
    growAnimationTime = 0.3f;

    float colorFactor = 1.0f + (attachedObjects.size() * 0.015f);
    if (colorFactor > 2.0f) colorFactor = 2.0f;
    ballColor = Vector4(0.2f * colorFactor, 0.7f, 0.2f, 1.0f);

    std::cout << "[Ball] Attached! Total: " << attachedObjects.size()
        << ", Radius: " << radius << " -> " << targetRadius
        << ", RelativeDepth: " << relativeDepth << std::endl;

    return true;
}

void KatamariBallGameComponent::UpdateAttachedObjects(float deltaTime) {
    // Вычисляем ось вращения
    Vector3 rotAxis = Vector3(1, 0, 0);

    float speed = velocity.Length();
    if (speed > 0.01f) {
        Vector3 moveDir = velocity;
        moveDir.y = 0;
        if (moveDir.Length() > 0.01f) {
            moveDir.Normalize();
            rotAxis = Vector3(0, 1, 0).Cross(moveDir);
            if (rotAxis.Length() < 0.1f) {
                rotAxis = Vector3(1, 0, 0);
            }
            else {
                rotAxis.Normalize();
            }
        }
    }

    // Текущее вращение шара
    Quaternion ballRotation = Quaternion::CreateFromAxisAngle(rotAxis, rotationAngle);

    for (auto& attached : attachedObjects) {
        if (attached.prop) {
            // Локальное направление пропа
            Vector3 localDir = attached.relativePosition;
            localDir.Normalize();

            // Поворачиваем локальное направление вместе с шаром
            Vector3 worldDir = Vector3::Transform(localDir, ballRotation);
            worldDir.Normalize();

            // Глубина должна масштабироваться относительно текущего радиуса
            // Сохраняем относительную глубину (от 0 до 1)
            float relativeDepth = attached.depth / radius;

            // Новая глубина с учетом текущего радиуса
            float currentDepth = radius * relativeDepth;

            // Новая позиция пропа с учетом глубины
            float attachRadius = radius - currentDepth;
            if (attachRadius < 0.1f) attachRadius = 0.1f;

            Vector3 newPos = position + worldDir * attachRadius;
            attached.prop->SetPosition(newPos);

            // Проп смотрит наружу от центра шара
            Vector3 up = worldDir;
            Vector3 forward;

            if (abs(up.y) < 0.99f) {
                forward = Vector3(0, 1, 0).Cross(up);
                forward.Normalize();
            }
            else {
                forward = Vector3(1, 0, 0);
            }

            Vector3 right = up.Cross(forward);
            right.Normalize();
            forward = right.Cross(up);
            forward.Normalize();

            Matrix rotMatrix;
            rotMatrix._11 = right.x; rotMatrix._12 = up.x; rotMatrix._13 = forward.x; rotMatrix._14 = 0;
            rotMatrix._21 = right.y; rotMatrix._22 = up.y; rotMatrix._23 = forward.y; rotMatrix._24 = 0;
            rotMatrix._31 = right.z; rotMatrix._32 = up.z; rotMatrix._33 = forward.z; rotMatrix._34 = 0;
            rotMatrix._41 = 0;       rotMatrix._42 = 0;    rotMatrix._43 = 0;         rotMatrix._44 = 1;

            Quaternion propRotation = Quaternion::CreateFromRotationMatrix(rotMatrix);

            // Применяем вращение шара к пропу
            propRotation = ballRotation * propRotation;

            attached.prop->GetModel().SetRotation(propRotation.ToEuler());
            attached.attachmentTime += deltaTime;
        }
    }
}

void KatamariBallGameComponent::Draw() {
    DrawBall();
    DrawDebugCollider();
}

void KatamariBallGameComponent::DrawBall() {
    if (!game || !game->Camera || !sphereInitialized) return;

    // Вычисляем ось вращения (ТАК ЖЕ, как в Update)
    Vector3 rotAxis = Vector3(1, 0, 0); // ось по умолчанию

    float speed = velocity.Length();
    if (speed > 0.01f) {
        Vector3 moveDir = velocity;
        moveDir.y = 0;
        if (moveDir.Length() > 0.01f) {
            moveDir.Normalize();
            rotAxis = Vector3(0, 1, 0).Cross(moveDir);
            if (rotAxis.Length() < 0.1f) {
                rotAxis = Vector3(1, 0, 0);
            }
            else {
                rotAxis.Normalize();
            }
        }
    }

    // Создаем вращение вокруг этой оси на накопленный угол
    Quaternion rotation = Quaternion::CreateFromAxisAngle(rotAxis, rotationAngle);

    Matrix world = Matrix::CreateScale(radius) *
        Matrix::CreateFromQuaternion(rotation) *
        Matrix::CreateTranslation(position);

    sphereRenderer.Draw(game, world, ballColor,
        game->Camera->GetViewMatrix(),
        game->Camera->GetProjectionMatrix(),
        ballTexture);
}

void KatamariBallGameComponent::DestroyResources() {
    if (sphereInitialized) {
        sphereRenderer.DestroyResources();
        sphereInitialized = false;
    }

    if (debugVertexBuffer) { debugVertexBuffer->Release(); debugVertexBuffer = nullptr; }
    if (debugIndexBuffer) { debugIndexBuffer->Release(); debugIndexBuffer = nullptr; }
    if (debugVS) { debugVS->Release(); debugVS = nullptr; }
    if (debugPS) { debugPS->Release(); debugPS = nullptr; }
    if (debugLayout) { debugLayout->Release(); debugLayout = nullptr; }
    if (debugConstantBuffer) { debugConstantBuffer->Release(); debugConstantBuffer = nullptr; }

    if (ballTexture) { ballTexture->Release(); ballTexture = nullptr; }

    debugInitialized = false;
}