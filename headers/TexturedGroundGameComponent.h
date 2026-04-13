#pragma once
#include "GameComponent.h"
#include "Camera.h"
#include <SimpleMath.h>
#include <vector>
#include <wincodec.h>
#include <comdef.h>

using namespace DirectX::SimpleMath;

class TexturedGroundGameComponent : public GameComponent {
private:
    struct Vertex {
        Vector3 position;
        Vector2 texCoord;
        Vector3 normal;
    };

    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    ID3D11InputLayout* inputLayout;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3D11Buffer* vsConstantBuffer;
    ID3D11Buffer* psConstantBuffer;
    ID3D11Buffer* materialBuffer;
    ID3D11Buffer* lightBuffer;
    ID3D11SamplerState* samplerState;
    ID3D11ShaderResourceView* textureView;

    UINT indexCount;
    bool initialized;
    bool textureLoaded;

    float size;
    int segments;
    std::string texturePath;

    void LoadTexture(Game* game, const std::string& path) {
        if (!game || !game->Device) return;
        if (textureLoaded) return;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

        if (SUCCEEDED(hr)) {
            Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
            std::wstring wpath(path.begin(), path.end());
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
                                    hr = game->Device->CreateShaderResourceView(tex, nullptr, &textureView);
                                    tex->Release();

                                    if (SUCCEEDED(hr) && textureView) {
                                        textureLoaded = true;
                                        std::cout << "[Ground] Texture loaded: " << path << std::endl;
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

public:
    TexturedGroundGameComponent(Game* game, float size = 100.0f, int segments = 100,
        const std::string& textureFile = "models/wood.jpg")
        : GameComponent(game)
        , size(size)
        , segments(segments)
        , texturePath(textureFile)
        , vertexBuffer(nullptr)
        , indexBuffer(nullptr)
        , inputLayout(nullptr)
        , vertexShader(nullptr)
        , pixelShader(nullptr)
        , vsConstantBuffer(nullptr)
        , psConstantBuffer(nullptr)
        , materialBuffer(nullptr)
        , lightBuffer(nullptr)
        , samplerState(nullptr)
        , textureView(nullptr)
        , indexCount(0)
        , initialized(false)
        , textureLoaded(false) {
    }

    ~TexturedGroundGameComponent() {
        DestroyResources();
    }

    void Initialize() override {
        if (initialized) return;

        CreateGround();
        CreateShaders();
        CreateBuffers();
        LoadTexture(game, texturePath);

        initialized = true;
        std::cout << "[Ground] Initialized! Size: " << size << ", Segments: " << segments << std::endl;
    }

    void Update(float deltaTime) override {
        // Обновление света
        UpdateLight(game, game->SunLight);
    }

    void Draw() override {
        if (!initialized || !game || !game->Context || !game->Camera) return;
        if (!vertexBuffer || !indexBuffer) return;

        // Обновляем VS константный буфер
        VSConstantBuffer vsCB;
        Matrix world = Matrix::Identity;
        vsCB.world = world.Transpose();
        vsCB.view = game->Camera->GetViewMatrix().Transpose();
        vsCB.projection = game->Camera->GetProjectionMatrix().Transpose();

        Matrix worldInv = world;
        worldInv.Invert();
        vsCB.worldInvTranspose = worldInv.Transpose();

        game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsCB, 0, 0);

        // Обновляем PS константный буфер
        PSConstantBuffer psCB;
        Vector3 camPos = game->Camera->GetPosition();
        psCB.cameraPosition = Vector4(camPos.x, camPos.y, camPos.z, 1.0f);
        psCB.objectColor = Vector4(1, 1, 1, 1);
        psCB.useTexture = textureLoaded ? 1 : 0;
        psCB.hasMaterial = 1;

        game->Context->UpdateSubresource(psConstantBuffer, 0, nullptr, &psCB, 0, 0);

        // Материал для пола (дерево)
        MaterialBuffer matBuffer;
        matBuffer.ambient = Vector4(0.3f, 0.3f, 0.3f, 1.0f);
        matBuffer.diffuse = Vector4(0.7f, 0.6f, 0.4f, 1.0f);
        matBuffer.specular = Vector4(0.2f, 0.2f, 0.2f, 1.0f);
        matBuffer.shininess = 16.0f;

        game->Context->UpdateSubresource(materialBuffer, 0, nullptr, &matBuffer, 0, 0);

        // Устанавливаем буферы
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        game->Context->IASetInputLayout(inputLayout);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);

        game->Context->PSSetShader(pixelShader, nullptr, 0);
        game->Context->PSSetConstantBuffers(0, 1, &psConstantBuffer);
        game->Context->PSSetConstantBuffers(1, 1, &materialBuffer);
        game->Context->PSSetConstantBuffers(2, 1, &lightBuffer);
        game->Context->PSSetSamplers(0, 1, &samplerState);

        if (textureLoaded && textureView) {
            game->Context->PSSetShaderResources(0, 1, &textureView);
        }

        game->Context->DrawIndexed(indexCount, 0, 0);
    }

    void DestroyResources() override {
        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
        if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
        if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
        if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
        if (vsConstantBuffer) { vsConstantBuffer->Release(); vsConstantBuffer = nullptr; }
        if (psConstantBuffer) { psConstantBuffer->Release(); psConstantBuffer = nullptr; }
        if (materialBuffer) { materialBuffer->Release(); materialBuffer = nullptr; }
        if (lightBuffer) { lightBuffer->Release(); lightBuffer = nullptr; }
        if (samplerState) { samplerState->Release(); samplerState = nullptr; }
        if (textureView) { textureView->Release(); textureView = nullptr; }
        initialized = false;
        textureLoaded = false;
    }

private:
    void UpdateLight(Game* game, const DirectionalLight& light) {
        if (!lightBuffer || !game || !game->Context) return;

        DirectionalLightBuffer lightBufferData;
        lightBufferData.ambient = light.ambient;
        lightBufferData.diffuse = light.diffuse;
        lightBufferData.specular = light.specular;
        lightBufferData.direction = light.direction;
        lightBufferData.padding = 0;

        game->Context->UpdateSubresource(lightBuffer, 0, nullptr, &lightBufferData, 0, 0);
    }

    void CreateGround() {
        std::vector<Vertex> vertices;
        std::vector<UINT> indices;

        float halfSize = size / 2.0f;
        float step = size / segments;

        for (int i = 0; i <= segments; i++) {
            float z = -halfSize + i * step;
            float v = (float)i / segments;

            for (int j = 0; j <= segments; j++) {
                float x = -halfSize + j * step;
                float u = (float)j / segments;

                Vertex vert;
                vert.position = Vector3(x, 0.0f, z);
                vert.texCoord = Vector2(u, v);
                vert.normal = Vector3(0, 1, 0); // Нормаль вверх
                vertices.push_back(vert);
            }
        }

        for (int i = 0; i < segments; i++) {
            for (int j = 0; j < segments; j++) {
                int first = i * (segments + 1) + j;
                int second = first + segments + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        indexCount = (UINT)indices.size();

        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = sizeof(Vertex) * (UINT)vertices.size();
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData = { vertices.data() };
        game->Device->CreateBuffer(&vertexDesc, &vertexData, &vertexBuffer);

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(UINT) * indexCount;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData = { indices.data() };
        game->Device->CreateBuffer(&indexDesc, &indexData, &indexBuffer);
    }

    void CreateShaders() {
        const char* vsCode = R"(
            cbuffer VSConstantBuffer : register(b0) {
                float4x4 world;
                float4x4 view;
                float4x4 projection;
                float4x4 worldInvTranspose;
            }

            struct VSInput {
                float3 position : POSITION;
                float2 texCoord : TEXCOORD;
                float3 normal : NORMAL;
            };

            struct VSOutput {
                float4 position : SV_POSITION;
                float2 texCoord : TEXCOORD0;
                float3 worldNormal : TEXCOORD1;
                float3 worldPosition : TEXCOORD2;
            };

            VSOutput VSMain(VSInput input) {
                VSOutput output;
                
                float4 worldPos = mul(float4(input.position, 1.0f), world);
                output.worldPosition = worldPos.xyz;
                output.position = mul(worldPos, view);
                output.position = mul(output.position, projection);
                
                output.worldNormal = mul(float4(input.normal, 0.0f), worldInvTranspose).xyz;
                output.worldNormal = normalize(output.worldNormal);
                
                output.texCoord = input.texCoord;
                
                return output;
            }
        )";

        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* error = nullptr;

        HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
            "VSMain", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0, &vsBlob, &error);

        if (FAILED(hr)) {
            if (error) error->Release();
            return;
        }

        const char* psCode = R"(
            cbuffer PSConstantBuffer : register(b0) {
                float4 cameraPosition;
                float4 objectColor;
                int useTexture;
                int hasMaterial;
                float2 padding;
            }
            
            cbuffer MaterialBuffer : register(b1) {
                float4 materialAmbient;
                float4 materialDiffuse;
                float4 materialSpecular;
                float shininess;
                float3 materialPadding;
            }
            
            cbuffer DirectionalLightBuffer : register(b2) {
                float4 lightAmbient;
                float4 lightDiffuse;
                float4 lightSpecular;
                float3 lightDirection;
                float lightPadding;
            }
            
            Texture2D objTexture : register(t0);
            SamplerState objSampler : register(s0);
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float2 texCoord : TEXCOORD0;
                float3 worldNormal : TEXCOORD1;
                float3 worldPosition : TEXCOORD2;
            };
            
            float4 PSMain(VSOutput input) : SV_TARGET {
                float3 normal = normalize(input.worldNormal);
                float3 lightDir = normalize(-lightDirection);
                float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
                float3 reflectDir = reflect(-lightDir, normal);
                
                float3 ambient = lightAmbient.rgb * materialAmbient.rgb;
                float diff = max(dot(normal, lightDir), 0.0f);
                float3 diffuse = lightDiffuse.rgb * diff * materialDiffuse.rgb;
                float spec = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);
                float3 specular = lightSpecular.rgb * spec * materialSpecular.rgb;
                float3 result = ambient + diffuse + specular;
                
                float4 texColor = float4(1, 1, 1, 1);
                if (useTexture != 0) {
                    texColor = objTexture.Sample(objSampler, input.texCoord);
                    result *= texColor.rgb;
                } else {
                    result *= materialDiffuse.rgb;
                }
                
                return float4(result, 1.0f);
            }
        )";

        ID3DBlob* psBlob = nullptr;
        hr = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr,
            "PSMain", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0, &psBlob, &error);

        if (FAILED(hr)) {
            if (error) error->Release();
            vsBlob->Release();
            return;
        }

        game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
        game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

        D3D11_INPUT_ELEMENT_DESC elements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        game->Device->CreateInputLayout(elements, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

        vsBlob->Release();
        psBlob->Release();
        if (error) error->Release();
    }

    void CreateBuffers() {
        D3D11_BUFFER_DESC desc = {};
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        desc.ByteWidth = sizeof(VSConstantBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &vsConstantBuffer);

        desc.ByteWidth = sizeof(PSConstantBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &psConstantBuffer);

        desc.ByteWidth = sizeof(MaterialBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &materialBuffer);

        desc.ByteWidth = sizeof(DirectionalLightBuffer);
        game->Device->CreateBuffer(&desc, nullptr, &lightBuffer);

        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        game->Device->CreateSamplerState(&samplerDesc, &samplerState);
    }
};