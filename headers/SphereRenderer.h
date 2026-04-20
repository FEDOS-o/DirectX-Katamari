#pragma once
#include "ShaderRenderer.h"
#include <vector>
#include <cmath>

namespace Render {

    class SphereRenderer : public ShaderRenderer {
    private:
        ID3D11Buffer* vertexBuffer = nullptr;
        ID3D11Buffer* indexBuffer = nullptr;
        ID3D11Buffer* shadowConstantBuffer = nullptr;
        UINT indexCount = 0;
        bool geometryInitialized = false;

        void CreateGeometry(Game* game, int segments, const Vector4& baseColor) {
            std::vector<Vertex> vertices;
            std::vector<UINT> indices;

            for (int lat = 0; lat <= segments; lat++) {
                float theta = lat * XM_PI / segments;
                float sinTheta = sin(theta);
                float cosTheta = cos(theta);

                for (int lon = 0; lon <= segments; lon++) {
                    float phi = lon * 2 * XM_PI / segments;
                    float sinPhi = sin(phi);
                    float cosPhi = cos(phi);

                    float x = cosPhi * sinTheta;
                    float y = cosTheta;
                    float z = sinPhi * sinTheta;

                    Vertex vert;
                    vert.position = Vector3(x, y, z);
                    vert.normal = Vector3(x, y, z);
                    vert.normal.Normalize();
                    vert.color = baseColor;
                    vert.texCoord = Vector2((float)lon / segments, (float)lat / segments);
                    vertices.push_back(vert);
                }
            }

            for (int lat = 0; lat < segments; lat++) {
                for (int lon = 0; lon < segments; lon++) {
                    int first = lat * (segments + 1) + lon;
                    int second = first + segments + 1;

                    indices.push_back(first);
                    indices.push_back(first + 1);
                    indices.push_back(second);
                    indices.push_back(second);
                    indices.push_back(first + 1);
                    indices.push_back(second + 1);
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

        void CreateShaders(Game* game) {
            const char* vsCode = R"(
            cbuffer VSConstantBuffer : register(b0) {
                float4x4 world;
                float4x4 view;
                float4x4 projection;
                float4x4 worldInvTranspose;
            }

            cbuffer ShadowConstantBuffer : register(b1) {
                float4x4 lightView;
                float4x4 lightProjection;
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
                float4 shadowPos : TEXCOORD3;
            };

            VSOutput VSMain(VSInput input) {
                VSOutput output;
                
                float4 worldPos = mul(float4(input.position, 1.0f), world);
                output.worldPosition = worldPos.xyz;
                output.position = mul(worldPos, view);
                output.position = mul(output.position, projection);
                
                output.worldNormal = mul(float4(input.normal, 0.0f), worldInvTranspose).xyz;
                output.worldNormal = normalize(output.worldNormal);
                
                output.color = input.color;
                output.texCoord = input.texCoord;
                
                float4 lightViewPos = mul(worldPos, lightView);
                output.shadowPos = mul(lightViewPos, lightProjection);
                
                return output;
            }
        )";

            const char* psCode = R"(
            cbuffer PSConstantBuffer : register(b0) {
                float4 cameraPosition;
                float4 objectColor;
                int useTexture;
                int hasMaterial;
                int useReflection;
                int useShadow;
                float shadowBias;
                float padding;
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
            TextureCube skyboxTexture : register(t1);
            Texture2D shadowMap : register(t2);
            SamplerState objSampler : register(s0);
            SamplerComparisonState shadowSampler : register(s1);
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
                float2 texCoord : TEXCOORD0;
                float3 worldNormal : TEXCOORD1;
                float3 worldPosition : TEXCOORD2;
                float4 shadowPos : TEXCOORD3;
            };
            
            float4 PSMain(VSOutput input) : SV_TARGET {
                float3 normal = normalize(input.worldNormal);
                float3 lightDir = normalize(-lightDirection);
                float3 viewDir = normalize(cameraPosition.xyz - input.worldPosition);
                float3 reflectLightDir = reflect(-lightDir, normal);
                
                float3 ambient = lightAmbient.rgb * materialAmbient.rgb;
                float diff = max(dot(normal, lightDir), 0.0f);
                float3 diffuse = lightDiffuse.rgb * diff * materialDiffuse.rgb;
                float spec = pow(max(dot(viewDir, reflectLightDir), 0.0f), shininess);
                float3 specular = lightSpecular.rgb * spec * materialSpecular.rgb;
                
                float shadowFactor = 1.0f;
                if (useShadow != 0) {
                    float3 projCoords = input.shadowPos.xyz / input.shadowPos.w;
                    projCoords.x = projCoords.x * 0.5f + 0.5f;
                    projCoords.y = projCoords.y * -0.5f + 0.5f;
                    
                    projCoords.z -= shadowBias;
                    
                    if (projCoords.x >= 0.0f && projCoords.x <= 1.0f &&
                        projCoords.y >= 0.0f && projCoords.y <= 1.0f &&
                        projCoords.z <= 1.0f) {
                        shadowFactor = shadowMap.SampleCmpLevelZero(shadowSampler, projCoords.xy, projCoords.z);
                        shadowFactor = saturate(shadowFactor + 0.1f);
                    }
                }
                
                float3 result = ambient + (diffuse + specular) * shadowFactor;
                
                if (useReflection != 0) {
                    float3 reflectDir = reflect(-viewDir, normal);
                    float3 reflection = skyboxTexture.Sample(objSampler, reflectDir).rgb;
                    result = result * 0.6f + reflection * 0.4f;
                }
                
                float4 texColor = float4(1, 1, 1, 1);
                if (useTexture != 0) {
                    texColor = objTexture.Sample(objSampler, input.texCoord);
                    result *= texColor.rgb;
                } else if (hasMaterial != 0) {
                    result *= materialDiffuse.rgb;
                } else {
                    result *= input.color.rgb;
                }
                
                return float4(result, 1.0f);
            }
        )";

            ID3DBlob* vsBlob = CompileShader(vsCode, "vs_5_0", "VSMain");
            ID3DBlob* psBlob = CompileShader(psCode, "ps_5_0", "PSMain");

            if (!vsBlob || !psBlob) {
                if (vsBlob) vsBlob->Release();
                if (psBlob) psBlob->Release();
                return;
            }

            game->Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
            game->Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);

            D3D11_INPUT_ELEMENT_DESC elements[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };

            game->Device->CreateInputLayout(elements, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);

            vsBlob->Release();
            psBlob->Release();
        }

    public:
        void Initialize(Game* game, const Vector4& baseColor, int segments = 32) {
            if (geometryInitialized) return;

            CreateGeometry(game, segments, baseColor);
            CreateShaders(game);

            // ÂÀÆÍÎ: Ñîçäàåì áóôåðû ÂÐÓ×ÍÓÞ ñ ïðàâèëüíûìè ðàçìåðàìè
            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

            desc.ByteWidth = sizeof(VSConstantBuffer);
            game->Device->CreateBuffer(&desc, nullptr, &vsConstantBuffer);

            // Ðàçìåð äîëæåí áûòü ÊÐÀÒÅÍ 16
            UINT psBufferSize = sizeof(PSConstantBuffer);
            UINT alignedSize = (psBufferSize + 15) & ~15;
            desc.ByteWidth = alignedSize;
            game->Device->CreateBuffer(&desc, nullptr, &psConstantBuffer);

            desc.ByteWidth = sizeof(MaterialBuffer);
            game->Device->CreateBuffer(&desc, nullptr, &materialBuffer);

            desc.ByteWidth = sizeof(DirectionalLightBuffer);
            game->Device->CreateBuffer(&desc, nullptr, &lightBuffer);

            desc.ByteWidth = sizeof(ShadowConstantBuffer);
            game->Device->CreateBuffer(&desc, nullptr, &shadowConstantBuffer);

            D3D11_SAMPLER_DESC samplerDesc = {};
            samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
            game->Device->CreateSamplerState(&samplerDesc, &samplerState);

            geometryInitialized = true;
            shadersInitialized = true;
        }

        void Draw(Game* game, const Matrix& world, const Vector4& color,
            const Matrix& view, const Matrix& projection,
            ID3D11ShaderResourceView* texture = nullptr,
            const Material* material = nullptr,
            bool useReflection = false,
            bool useShadow = true) {

            if (!geometryInitialized || !game || !game->Context) return;

            UpdateLightBuffer(game, game->SunLight);

            // VS áóôåð
            VSConstantBuffer vsCB;
            vsCB.world = world.Transpose();
            vsCB.view = view.Transpose();
            vsCB.projection = projection.Transpose();
            Matrix worldInv = world;
            worldInv.Invert();
            vsCB.worldInvTranspose = worldInv.Transpose();
            game->Context->UpdateSubresource(vsConstantBuffer, 0, nullptr, &vsCB, 0, 0);

            // Shadow áóôåð
            Matrix lightView = game->GetLightViewMatrix();
            Matrix lightProjection = game->GetLightProjectionMatrix();
            ShadowConstantBuffer shadowCB;
            shadowCB.lightView = lightView.Transpose();
            shadowCB.lightProjection = lightProjection.Transpose();
            game->Context->UpdateSubresource(shadowConstantBuffer, 0, nullptr, &shadowCB, 0, 0);

            // PS áóôåð - ÏÐßÌÎÅ ÈÑÏÎËÜÇÎÂÀÍÈÅ ÑÒÐÓÊÒÓÐÛ ÁÅÇ ÂÛÐÀÂÍÈÂÀÍÈß
            // Áóôåð óæå ñîçäàí ñ âûðîâíåííûì ðàçìåðîì, äàííûå ìîæíî ïåðåäàâàòü êàê åñòü
            PSConstantBuffer psCB;
            Vector3 camPos = game->Camera->GetPosition();
            psCB.cameraPosition = DirectX::XMFLOAT4(camPos.x, camPos.y, camPos.z, 1.0f);
            psCB.objectColor = DirectX::XMFLOAT4(color.x, color.y, color.z, color.w);
            psCB.useTexture = (texture != nullptr) ? 1 : 0;
            psCB.hasMaterial = (material != nullptr) ? 1 : 0;
            psCB.useReflection = useReflection ? 1 : 0;
            psCB.useShadow = (useShadow && game->ShadowMapSRV) ? 1 : 0;
            psCB.shadowBias = game->ShadowBias;
            psCB.padding = 0.0f;

            game->Context->UpdateSubresource(psConstantBuffer, 0, nullptr, &psCB, 0, 0);

            Material defaultMaterial(color, 32.0f);
            const Material* matToUse = material ? material : &defaultMaterial;

            MaterialBuffer matBuffer;
            matBuffer.ambient = matToUse->ambient;
            matBuffer.diffuse = matToUse->diffuse;
            matBuffer.specular = matToUse->specular;
            matBuffer.shininess = matToUse->shininess;
            matBuffer.materialPadding[0] = matBuffer.materialPadding[1] = matBuffer.materialPadding[2] = 0.0f;
            game->Context->UpdateSubresource(materialBuffer, 0, nullptr, &matBuffer, 0, 0);

            UINT stride = sizeof(Vertex);
            UINT offset = 0;
            game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            game->Context->IASetInputLayout(inputLayout);
            game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            game->Context->VSSetShader(vertexShader, nullptr, 0);
            game->Context->VSSetConstantBuffers(0, 1, &vsConstantBuffer);
            game->Context->VSSetConstantBuffers(1, 1, &shadowConstantBuffer);

            game->Context->PSSetShader(pixelShader, nullptr, 0);
            game->Context->PSSetConstantBuffers(0, 1, &psConstantBuffer);
            game->Context->PSSetConstantBuffers(1, 1, &materialBuffer);
            game->Context->PSSetConstantBuffers(2, 1, &lightBuffer);
            game->Context->PSSetSamplers(0, 1, &samplerState);

            if (texture) {
                game->Context->PSSetShaderResources(0, 1, &texture);
            }

            if (useReflection && game->SkyboxTexture) {
                game->Context->PSSetShaderResources(1, 1, &game->SkyboxTexture);
            }

            if (useShadow && game->ShadowMapSRV) {
                game->Context->PSSetShaderResources(2, 1, &game->ShadowMapSRV);
                game->Context->PSSetSamplers(1, 1, &game->ShadowSampler);
            }

            game->Context->DrawIndexed(indexCount, 0, 0);
        }

        void DestroyResources() override {
            ShaderRenderer::DestroyResources();
            if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
            if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
            if (shadowConstantBuffer) { shadowConstantBuffer->Release(); shadowConstantBuffer = nullptr; }
            geometryInitialized = false;
        }

        ID3D11Buffer* GetVertexBuffer() const { return vertexBuffer; }
        ID3D11Buffer* GetIndexBuffer() const { return indexBuffer; }
        UINT GetIndexCount() const { return indexCount; }
    };

} // namespace Render