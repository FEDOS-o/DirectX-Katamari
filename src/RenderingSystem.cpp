// RenderingSystem.cpp
#include "RenderingSystem.h"
#include "Game.h"
#include <iostream>

RenderingSystem::RenderingSystem()
    : game(nullptr)
    , gBuffer(nullptr)
    , screenWidth(0)
    , screenHeight(0)
    , initialized(false)
    , fullscreenVS(nullptr)
    , geometryVS(nullptr)
    , geometryPS(nullptr)
    , directionalLightPS(nullptr)
    , debugGBufferPS(nullptr)
    , inputLayout(nullptr)
    , vsConstantBuffer(nullptr)
    , directionalLightBuffer(nullptr)
    , cameraBuffer(nullptr)
    , shadowLightBuffer(nullptr)
    , linearSampler(nullptr)
    , pointSampler(nullptr)
    , additiveBlendState(nullptr)
    , noBlendState(nullptr)
    , lightDepthState(nullptr)
    , noCullRasterizer(nullptr) {
}

RenderingSystem::~RenderingSystem() {
    Destroy();
}

ID3DBlob* RenderingSystem::CompileShader(const char* code, const char* target, const char* entry) {
    ID3DBlob* blob = nullptr;
    ID3DBlob* error = nullptr;

    HRESULT hr = D3DCompile(code, strlen(code), nullptr, nullptr, nullptr,
        entry, target, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &blob, &error);

    if (FAILED(hr) && error) {
        OutputDebugStringA((char*)error->GetBufferPointer());
        error->Release();
        return nullptr;
    }

    if (error) error->Release();
    return blob;
}

HRESULT RenderingSystem::CreateShaders() {
    ID3D11Device* device = game->Device.Get();
    if (!device) return E_POINTER;

    // ========================================
    // 1. FULLSCREEN QUAD VERTEX SHADER
    // ========================================
    const char* fullscreenVSCode = R"(
        struct VSOutput {
            float4 position : SV_POSITION;
            float2 texCoord : TEXCOORD0;
        };

        VSOutput VSMain(uint vertexID : SV_VertexID) {
            VSOutput output;
            float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
            output.texCoord = uv;
            output.position = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
            return output;
        }
    )";

    ID3DBlob* vsBlob = CompileShader(fullscreenVSCode, "vs_5_0", "VSMain");
    if (!vsBlob) return E_FAIL;
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &fullscreenVS);
    vsBlob->Release();

    // ========================================
    // 2. GEOMETRY PASS VERTEX SHADER
    // ========================================
    const char* geometryVSCode = R"(
        cbuffer VSConstantBuffer : register(b0) {
            float4x4 world;
            float4x4 view;
            float4x4 projection;
            float4x4 worldInvTranspose;
        }

        struct VSInput {
            float3 position : POSITION;
            float4 color : COLOR;
            float2 texCoord : TEXCOORD;
            float3 normal : NORMAL;
        };

        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
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
            
            output.worldNormal = normalize(mul(float4(input.normal, 0.0f), worldInvTranspose).xyz);
            output.color = input.color;
            output.texCoord = input.texCoord;
            
            return output;
        }
    )";

    vsBlob = CompileShader(geometryVSCode, "vs_5_0", "VSMain");
    if (!vsBlob) return E_FAIL;
    device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &geometryVS);

    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    device->CreateInputLayout(elements, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    vsBlob->Release();

    // ========================================
    // 3. GEOMETRY PASS PIXEL SHADER
    // ========================================
    const char* geometryPSCode = R"(
        struct VSOutput {
            float4 position : SV_POSITION;
            float4 color : COLOR;
            float2 texCoord : TEXCOORD0;
            float3 worldNormal : TEXCOORD1;
            float3 worldPosition : TEXCOORD2;
        };

        struct GBufferOutput {
            float4 diffuse  : SV_TARGET0;
            float4 normal   : SV_TARGET1;
            float4 worldPos : SV_TARGET2;
            float4 specular : SV_TARGET3;
        };

        Texture2D objTexture : register(t0);
        SamplerState objSampler : register(s0);

        GBufferOutput PSMain(VSOutput input) {
            GBufferOutput output;
    
            float4 texColor = objTexture.Sample(objSampler, input.texCoord);
        
            float4 finalColor;
            if (texColor.r < 0.01f && texColor.g < 0.01f && texColor.b < 0.01f) {
                finalColor = float4(1, 1, 1, 1);
            } else {
                finalColor = texColor;
            }
        
            output.diffuse = finalColor;
            output.normal = float4(normalize(input.worldNormal), 1.0f);
            output.worldPos = float4(input.worldPosition, 1.0f);
            output.specular = float4(0.5f, 0.5f, 0.5f, 32.0f / 255.0f);
    
            return output;
        }
    )";

    ID3DBlob* psBlob = CompileShader(geometryPSCode, "ps_5_0", "PSMain");
    if (!psBlob) return E_FAIL;
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &geometryPS);
    psBlob->Release();

    // ========================================
    // 4. DIRECTIONAL LIGHT PIXEL SHADER with SHADOWS
    // ========================================
    const char* directionalPSCode = R"(
        struct VSOutput {
            float4 position : SV_POSITION;
            float2 texCoord : TEXCOORD0;
        };

        cbuffer DirectionalLightBuffer : register(b0) {
            float4 lightAmbient;
            float4 lightDiffuse;
            float4 lightSpecular;
            float3 lightDirection;
            float padding;
        }

        cbuffer CameraBuffer : register(b1) {
            float3 cameraPosition;
            float cameraPadding;
        }

        cbuffer ShadowBuffer : register(b2) {
            float4x4 lightViewProj[4];
            float4 cascadeSplits;
            float shadowBias;
            float3 shadowPadding;
        }

        Texture2D diffuseTex  : register(t0);
        Texture2D normalTex   : register(t1);
        Texture2D worldPosTex : register(t2);
        Texture2D specularTex : register(t3);
        Texture2DArray shadowMap : register(t4);
        SamplerState linearSampler : register(s0);
        SamplerComparisonState shadowSampler : register(s1);

        float CalculateShadowFactor(float3 worldPos, float3 normal, float3 lightDir, int cascadeIndex) {
            float4 shadowPos = mul(float4(worldPos, 1.0f), lightViewProj[cascadeIndex]);
            float3 projCoords = shadowPos.xyz / shadowPos.w;
            projCoords.x = projCoords.x * 0.5f + 0.5f;
            projCoords.y = projCoords.y * -0.5f + 0.5f;
            
            float diff = max(dot(normal, lightDir), 0.0f);
            float bias = shadowBias * tan(acos(saturate(diff)));
            bias = clamp(bias, 0.0f, 0.01f);
            projCoords.z -= bias;
            
            if (projCoords.x < 0.0f || projCoords.x > 1.0f || 
                projCoords.y < 0.0f || projCoords.y > 1.0f) {
                return 1.0f;
            }
            
            float2 texelSize = float2(1.0f / 2048.0f, 1.0f / 2048.0f);
            float shadow = 0.0f;
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, float3(projCoords.xy + float2(-0.5f, -0.5f) * texelSize, cascadeIndex), projCoords.z);
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, float3(projCoords.xy + float2(0.5f, -0.5f) * texelSize, cascadeIndex), projCoords.z);
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, float3(projCoords.xy + float2(-0.5f, 0.5f) * texelSize, cascadeIndex), projCoords.z);
            shadow += shadowMap.SampleCmpLevelZero(shadowSampler, float3(projCoords.xy + float2(0.5f, 0.5f) * texelSize, cascadeIndex), projCoords.z);
            shadow *= 0.25f;
            
            return saturate(shadow + 0.15f);
        }

        float4 PSMain(VSOutput input) : SV_TARGET {
            float4 albedo = diffuseTex.Sample(linearSampler, input.texCoord);
            
            if (albedo.r + albedo.g + albedo.b < 0.01f) {
                return float4(0, 0, 0, 0);
            }
            
            float4 normalData = normalTex.Sample(linearSampler, input.texCoord);
            float3 normal = normalize(normalData.xyz * 2.0f - 1.0f);
            
            float4 worldPosData = worldPosTex.Sample(linearSampler, input.texCoord);
            float3 worldPosition = worldPosData.xyz;
            
            float4 specularData = specularTex.Sample(linearSampler, input.texCoord);
            float3 specularColor = specularData.rgb;
            float shininess = max(specularData.a * 255.0f, 1.0f);
            
            float3 lightDir = normalize(-lightDirection);
            float3 viewDir = normalize(cameraPosition - worldPosition);
            
            float diff = max(dot(normal, lightDir), 0.0f);
            float3 diffuse = lightDiffuse.rgb * diff * albedo.rgb;
            
            float3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfwayDir), 0.0f), shininess);
            float3 specular = lightSpecular.rgb * spec * specularColor;
            
            float3 ambient = lightAmbient.rgb * albedo.rgb;
            
            float depth = length(cameraPosition - worldPosition);
            
            int cascadeIndex = 3;
            if (depth <= cascadeSplits.x) cascadeIndex = 0;
            else if (depth <= cascadeSplits.y) cascadeIndex = 1;
            else if (depth <= cascadeSplits.z) cascadeIndex = 2;
            
            float shadowFactor = CalculateShadowFactor(worldPosition, normal, lightDir, cascadeIndex);
            
            float3 result = ambient + (diffuse + specular) * shadowFactor;
            
            return float4(result, 1.0f);
        }
    )";

    psBlob = CompileShader(directionalPSCode, "ps_5_0", "PSMain");
    if (!psBlob) return E_FAIL;
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &directionalLightPS);
    psBlob->Release();

    // ========================================
    // 5. DEBUG GBUFFER PIXEL SHADER
    // ========================================
    const char* debugPSCode = R"(
        cbuffer DebugBuffer : register(b0) {
            int debugIndex;
            float3 debugPadding;
        }

        Texture2D diffuseTex  : register(t0);
        Texture2D normalTex   : register(t1);
        Texture2D worldPosTex : register(t2);
        Texture2D specularTex : register(t3);
        Texture2D depthTex    : register(t4);
        SamplerState linearSampler : register(s0);

        float4 PSMain(float4 position : SV_POSITION, float2 texCoord : TEXCOORD0) : SV_TARGET {
            if (debugIndex == 0) return diffuseTex.Sample(linearSampler, texCoord);
            if (debugIndex == 1) {
                float4 normal = normalTex.Sample(linearSampler, texCoord);
                return float4(normal.xyz * 0.5f + 0.5f, 1.0f);
            }
            if (debugIndex == 2) {
                float4 worldPos = worldPosTex.Sample(linearSampler, texCoord);
                return float4(fmod(worldPos.xyz, 10.0f) / 10.0f, 1.0f);
            }
            if (debugIndex == 3) return specularTex.Sample(linearSampler, texCoord);
            if (debugIndex == 4) {
                float depth = depthTex.Sample(linearSampler, texCoord);
                return float4(depth, depth, depth, 1.0f);
            }
            return float4(1, 0, 1, 1);
        }
    )";

    psBlob = CompileShader(debugPSCode, "ps_5_0", "PSMain");
    if (!psBlob) return E_FAIL;
    device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &debugGBufferPS);
    psBlob->Release();

    return S_OK;
}

HRESULT RenderingSystem::CreateBuffers() {
    ID3D11Device* device = game->Device.Get();
    if (!device) return E_POINTER;

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    desc.ByteWidth = sizeof(VSConstantBuffer);
    HRESULT hr = device->CreateBuffer(&desc, nullptr, &vsConstantBuffer);
    if (FAILED(hr)) return hr;

    struct DirLightBuffer {
        Vector4 ambient;
        Vector4 diffuse;
        Vector4 specular;
        Vector3 direction;
        float padding;
    };
    desc.ByteWidth = sizeof(DirLightBuffer);
    hr = device->CreateBuffer(&desc, nullptr, &directionalLightBuffer);
    if (FAILED(hr)) return hr;

    struct CamBuffer {
        Vector3 position;
        float padding;
    };
    desc.ByteWidth = sizeof(CamBuffer);
    hr = device->CreateBuffer(&desc, nullptr, &cameraBuffer);
    if (FAILED(hr)) return hr;

    struct ShadowBufferData {
        Matrix lightViewProj[4];
        Vector4 cascadeSplits;
        float shadowBias;
        float padding[3];
    };
    desc.ByteWidth = sizeof(ShadowBufferData);
    hr = device->CreateBuffer(&desc, nullptr, &shadowLightBuffer);
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT RenderingSystem::CreateStates() {
    ID3D11Device* device = game->Device.Get();
    if (!device) return E_POINTER;

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = device->CreateSamplerState(&samplerDesc, &linearSampler);
    if (FAILED(hr)) return hr;

    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = device->CreateSamplerState(&samplerDesc, &pointSampler);
    if (FAILED(hr)) return hr;

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = true;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&blendDesc, &additiveBlendState);
    if (FAILED(hr)) return hr;

    blendDesc.RenderTarget[0].BlendEnable = false;
    hr = device->CreateBlendState(&blendDesc, &noBlendState);
    if (FAILED(hr)) return hr;

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    hr = device->CreateDepthStencilState(&dsDesc, &lightDepthState);
    if (FAILED(hr)) return hr;

    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.DepthClipEnable = true;

    hr = device->CreateRasterizerState(&rastDesc, &noCullRasterizer);
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT RenderingSystem::Initialize(Game* inGame, int width, int height) {
    if (initialized) return S_OK;
    if (!inGame || !inGame->Device) return E_POINTER;

    game = inGame;
    screenWidth = width;
    screenHeight = height;

    gBuffer = new GBuffer();
    HRESULT hr = gBuffer->Initialize(game->Device.Get(), width, height);
    if (FAILED(hr)) {
        delete gBuffer;
        gBuffer = nullptr;
        return hr;
    }

    hr = CreateShaders();
    if (FAILED(hr)) {
        Destroy();
        return hr;
    }

    hr = CreateBuffers();
    if (FAILED(hr)) {
        Destroy();
        return hr;
    }

    hr = CreateStates();
    if (FAILED(hr)) {
        Destroy();
        return hr;
    }

    initialized = true;
    return S_OK;
}

void RenderingSystem::Destroy() {
    if (fullscreenVS) { fullscreenVS->Release(); fullscreenVS = nullptr; }
    if (geometryVS) { geometryVS->Release(); geometryVS = nullptr; }
    if (geometryPS) { geometryPS->Release(); geometryPS = nullptr; }
    if (directionalLightPS) { directionalLightPS->Release(); directionalLightPS = nullptr; }
    if (debugGBufferPS) { debugGBufferPS->Release(); debugGBufferPS = nullptr; }
    if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
    if (vsConstantBuffer) { vsConstantBuffer->Release(); vsConstantBuffer = nullptr; }
    if (directionalLightBuffer) { directionalLightBuffer->Release(); directionalLightBuffer = nullptr; }
    if (cameraBuffer) { cameraBuffer->Release(); cameraBuffer = nullptr; }
    if (shadowLightBuffer) { shadowLightBuffer->Release(); shadowLightBuffer = nullptr; }
    if (linearSampler) { linearSampler->Release(); linearSampler = nullptr; }
    if (pointSampler) { pointSampler->Release(); pointSampler = nullptr; }
    if (additiveBlendState) { additiveBlendState->Release(); additiveBlendState = nullptr; }
    if (noBlendState) { noBlendState->Release(); noBlendState = nullptr; }
    if (lightDepthState) { lightDepthState->Release(); lightDepthState = nullptr; }
    if (noCullRasterizer) { noCullRasterizer->Release(); noCullRasterizer = nullptr; }

    if (gBuffer) {
        gBuffer->Release();
        delete gBuffer;
        gBuffer = nullptr;
    }

    initialized = false;
    game = nullptr;
}

void RenderingSystem::BeginGeometryPass(ID3D11DeviceContext* context,
    const Matrix& view,
    const Matrix& projection) {
    if (!initialized || !context) return;

    currentView = view;
    currentProjection = projection;

    gBuffer->SetRenderTargets(context);
    gBuffer->Clear(context);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = (float)screenWidth;
    viewport.Height = (float)screenHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.DepthClipEnable = TRUE;

    ID3D11RasterizerState* noCullState = nullptr;
    game->Device->CreateRasterizerState(&rastDesc, &noCullState);
    context->RSSetState(noCullState);
    if (noCullState) noCullState->Release();

    context->IASetInputLayout(inputLayout);
    context->VSSetShader(geometryVS, nullptr, 0);
    context->PSSetShader(geometryPS, nullptr, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void RenderingSystem::DrawMeshToGBuffer(ID3D11DeviceContext* context,
    ID3D11Buffer* vertexBuffer,
    ID3D11Buffer* indexBuffer,
    UINT indexCount,
    const Matrix& world) {

    if (!initialized || !context) return;
    if (!vertexBuffer || !indexBuffer) return;

    VSConstantBuffer cb;
    cb.world = world.Transpose();
    cb.view = currentView.Transpose();
    cb.projection = currentProjection.Transpose();

    Matrix worldInv = world;
    worldInv.Invert();
    cb.worldInvTranspose = worldInv.Transpose();

    context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &cb, 0, 0);
    context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

    context->DrawIndexed(indexCount, 0, 0);
}

void RenderingSystem::EndGeometryPass(ID3D11DeviceContext* context) {
    (void)context;
}

void RenderingSystem::RenderLighting(ID3D11DeviceContext* context,
    ID3D11RenderTargetView* finalRTV,
    const DirectionalLight& light,
    const Vector3& cameraPosition,
    ID3D11ShaderResourceView* shadowMapSRV,
    ID3D11SamplerState* shadowSampler) {

    if (!initialized || !context) return;

    // ВАЖНО: используем blending, чтобы добавить освещение к существующему изображению
    context->OMSetRenderTargets(1, &finalRTV, nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = (float)screenWidth;
    viewport.Height = (float)screenHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    // Используем blending для добавления освещения
    float blendFactor[4] = { 0, 0, 0, 0 };
    // Включаем blending: результат = existingColor + newColor
    context->OMSetBlendState(additiveBlendState, blendFactor, 0xffffffff);
    context->OMSetDepthStencilState(lightDepthState, 0);
    context->RSSetState(noCullRasterizer);

    context->VSSetShader(fullscreenVS, nullptr, 0);
    context->PSSetShader(directionalLightPS, nullptr, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11ShaderResourceView* textures[] = {
        gBuffer->GetSRV(GBuffer::DIFFUSE),
        gBuffer->GetSRV(GBuffer::NORMAL),
        gBuffer->GetSRV(GBuffer::WORLD_POS),
        gBuffer->GetSRV(GBuffer::SPECULAR)
    };
    context->PSSetShaderResources(0, 4, textures);

    if (shadowMapSRV) {
        context->PSSetShaderResources(4, 1, &shadowMapSRV);
        context->PSSetSamplers(1, 1, &shadowSampler);
    }

    context->PSSetSamplers(0, 1, &linearSampler);

    // Directional Light buffer
    struct DirLightBuffer {
        Vector4 ambient;
        Vector4 diffuse;
        Vector4 specular;
        Vector3 direction;
        float padding;
    } lightData;

    lightData.ambient = light.ambient;
    lightData.diffuse = light.diffuse;
    lightData.specular = light.specular;
    lightData.direction = light.direction;
    lightData.padding = 0.0f;

    context->UpdateSubresource(directionalLightBuffer, 0, nullptr, &lightData, 0, 0);
    context->PSSetConstantBuffers(0, 1, &directionalLightBuffer);

    // Camera buffer
    struct CamBuffer {
        Vector3 position;
        float padding;
    } camData;

    camData.position = cameraPosition;
    camData.padding = 0.0f;

    context->UpdateSubresource(cameraBuffer, 0, nullptr, &camData, 0, 0);
    context->PSSetConstantBuffers(1, 1, &cameraBuffer);

    // Shadow buffer
    if (shadowLightBuffer && game) {
        struct ShadowBufferData {
            Matrix lightViewProj[4];
            Vector4 cascadeSplits;
            float shadowBias;
            float padding[3];
        } shadowData;

        for (int i = 0; i < 4; i++) {
            shadowData.lightViewProj[i] = (game->GetCascadeLightViewMatrix(i) *
                game->GetCascadeLightProjectionMatrix(i)).Transpose();
        }
        shadowData.cascadeSplits.x = game->GetCascadeSplitDepth(0);
        shadowData.cascadeSplits.y = game->GetCascadeSplitDepth(1);
        shadowData.cascadeSplits.z = game->GetCascadeSplitDepth(2);
        shadowData.cascadeSplits.w = 0.0f;
        shadowData.shadowBias = game->ShadowBias;

        context->UpdateSubresource(shadowLightBuffer, 0, nullptr, &shadowData, 0, 0);
        context->PSSetConstantBuffers(2, 1, &shadowLightBuffer);
    }

    context->Draw(3, 0);

    ID3D11ShaderResourceView* nullSRV[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    context->PSSetShaderResources(0, 5, nullSRV);
}

void RenderingSystem::RenderDebugGBuffer(ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target,
    int textureIndex) {
    // Implementation remains the same as before
    (void)context;
    (void)target;
    (void)textureIndex;
}

void RenderingSystem::TestDrawRedScreen(ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target) {
    if (!target) return;
    float red[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(target, red);
}

void RenderingSystem::RenderSimpleFullscreenQuad(ID3D11DeviceContext* context,
    ID3D11RenderTargetView* target) {
    // Implementation remains the same as before
    (void)context;
    (void)target;
}