#pragma once
#include "Game.h"
#include <d3dcompiler.h>
#include <iostream>

#pragma comment(lib, "d3dcompiler.lib")

namespace Render {

    class ShadowRenderer {
    private:
        ID3D11VertexShader* vertexShader = nullptr;
        ID3D11InputLayout* inputLayout = nullptr;
        ID3D11RasterizerState* rasterizerState = nullptr;
        ID3D11DepthStencilState* depthStencilState = nullptr;
        bool initialized = false;

    public:
        ~ShadowRenderer() {
            if (vertexShader) vertexShader->Release();
            if (inputLayout) inputLayout->Release();
            if (rasterizerState) rasterizerState->Release();
            if (depthStencilState) depthStencilState->Release();
        }

        void Initialize(Game* game) {
            if (initialized) return;

            const char* vsCode = R"(
                cbuffer ShadowConstantBuffer : register(b0) {
                    matrix lightViewProj;
                };

                cbuffer WorldConstantBuffer : register(b1) {
                    matrix world;
                };

                struct VSInput {
                    float3 position : POSITION;
                };

                struct VSOutput {
                    float4 position : SV_POSITION;
                };

                VSOutput VSMain(VSInput input) {
                    VSOutput output;
                    
                    float4 worldPos = mul(float4(input.position, 1.0f), world);
                    output.position = mul(worldPos, lightViewProj);
                    
                    return output;
                }
            )";

            ID3DBlob* vsBlob = nullptr;
            ID3DBlob* error = nullptr;

            HRESULT hr = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr,
                "VSMain", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vsBlob, &error);

            if (FAILED(hr)) {
                if (error) {
                    OutputDebugStringA((char*)error->GetBufferPointer());
                    error->Release();
                }
                std::cout << "ShadowRenderer: Failed to compile vertex shader!" << std::endl;
                return;
            }

            hr = game->Device->CreateVertexShader(vsBlob->GetBufferPointer(),
                vsBlob->GetBufferSize(), nullptr, &vertexShader);

            if (SUCCEEDED(hr)) {
                D3D11_INPUT_ELEMENT_DESC elements[] = {
                    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                     D3D11_INPUT_PER_VERTEX_DATA, 0}
                };

                game->Device->CreateInputLayout(elements, 1, vsBlob->GetBufferPointer(),
                    vsBlob->GetBufferSize(), &inputLayout);
            }

            vsBlob->Release();

            D3D11_RASTERIZER_DESC rastDesc = {};
            rastDesc.CullMode = D3D11_CULL_BACK;
            rastDesc.FillMode = D3D11_FILL_SOLID;
            rastDesc.DepthClipEnable = true;
            rastDesc.DepthBias = 10000;
            rastDesc.DepthBiasClamp = 0.0f;
            rastDesc.SlopeScaledDepthBias = 4.0f;
            rastDesc.FrontCounterClockwise = false;

            game->Device->CreateRasterizerState(&rastDesc, &rasterizerState);

            D3D11_DEPTH_STENCIL_DESC dsDesc = {};
            dsDesc.DepthEnable = true;
            dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
            dsDesc.StencilEnable = false;

            game->Device->CreateDepthStencilState(&dsDesc, &depthStencilState);

            initialized = true;
            std::cout << "ShadowRenderer initialized successfully" << std::endl;
        }

        void BeginShadowPass(Game* game) {
            if (!initialized) return;

            game->Context->VSSetShader(vertexShader, nullptr, 0);
            game->Context->PSSetShader(nullptr, nullptr, 0);
            game->Context->GSSetShader(nullptr, nullptr, 0);
            game->Context->IASetInputLayout(inputLayout);
            game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            game->Context->RSSetState(rasterizerState);
            game->Context->OMSetDepthStencilState(depthStencilState, 0);
        }

        void DrawMesh(Game* game, ID3D11Buffer* vertexBuffer, UINT vertexCount,
            ID3D11Buffer* indexBuffer, UINT indexCount, const Matrix& world) {
            if (!initialized) return;
            if (!game->shadowWorldConstantBuffer) return;

            ShadowWorldConstantBuffer cb;
            cb.world = world.Transpose();
            game->Context->UpdateSubresource(game->shadowWorldConstantBuffer, 0, nullptr, &cb, 0, 0);
            game->Context->VSSetConstantBuffers(1, 1, &game->shadowWorldConstantBuffer);

            // ВАЖНО: stride = 12 (только позиция x,y,z)
            UINT stride = 12;
            UINT offset = 0;
            game->Context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            game->Context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            game->Context->DrawIndexed(indexCount, 0, 0);
        }

        void EndShadowPass(Game* game) {
            game->Context->RSSetState(game->RasterizerState);
            game->Context->OMSetDepthStencilState(game->DepthStencilState, 1);
        }
    };

} // namespace Render