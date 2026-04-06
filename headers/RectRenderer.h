#pragma once
#include "Game.h"
#include "Rect.h"

class RectangleRenderer {
protected:
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    ID3D11InputLayout* inputLayout;
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    bool initialized;

public:
    RectangleRenderer() : vertexBuffer(nullptr), indexBuffer(nullptr), 
                          inputLayout(nullptr), vertexShader(nullptr), 
                          pixelShader(nullptr), initialized(false) {}
    
    virtual ~RectangleRenderer() {
        DestroyResources();
    }
    
    void Initialize(Game* game) {
        if (initialized) return;
        
        ID3DBlob* vertexBC = nullptr;
        ID3DBlob* errorVertexCode = nullptr;
        
        const char* vertexShaderCode = R"(
            struct VSInput {
                float4 position : POSITION;
                float4 color : COLOR;
            };
            
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
            };
            
            VSOutput VSMain(VSInput input) {
                VSOutput output;
                output.position = input.position;
                output.color = input.color;
                return output;
            }
        )";
        
        HRESULT hr = D3DCompile(vertexShaderCode, strlen(vertexShaderCode), 
                                 nullptr, nullptr, nullptr, "VSMain", "vs_5_0",
                                 D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
                                 0, &vertexBC, &errorVertexCode);
        
        if (FAILED(hr)) {
            if (errorVertexCode) {
                char* compileErrors = (char*)errorVertexCode->GetBufferPointer();
                std::cout << "Vertex shader error: " << compileErrors << std::endl;
                errorVertexCode->Release();
            }
            return;
        }
        
        const char* pixelShaderCode = R"(
            struct VSOutput {
                float4 position : SV_POSITION;
                float4 color : COLOR;
            };
            
            float4 PSMain(VSOutput input) : SV_TARGET {
                return input.color;
            }
        )";
        
        ID3DBlob* pixelBC = nullptr;
        ID3DBlob* errorPixelCode = nullptr;
        
        hr = D3DCompile(pixelShaderCode, strlen(pixelShaderCode), 
                        nullptr, nullptr, nullptr, "PSMain", "ps_5_0",
                        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
                        0, &pixelBC, &errorPixelCode);
        
        if (FAILED(hr)) {
            if (errorPixelCode) {
                char* compileErrors = (char*)errorPixelCode->GetBufferPointer();
                std::cout << "Pixel shader error: " << compileErrors << std::endl;
                errorPixelCode->Release();
            }
            vertexBC->Release();
            return;
        }
        
        game->Device->CreateVertexShader(vertexBC->GetBufferPointer(), 
                                          vertexBC->GetBufferSize(), 
                                          nullptr, &vertexShader);
        
        game->Device->CreatePixelShader(pixelBC->GetBufferPointer(), 
                                         pixelBC->GetBufferSize(), 
                                         nullptr, &pixelShader);
        
        D3D11_INPUT_ELEMENT_DESC inputElements[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, 
             D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 
             D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };
        
        game->Device->CreateInputLayout(inputElements, 2,
                                        vertexBC->GetBufferPointer(),
                                        vertexBC->GetBufferSize(),
                                        &inputLayout);
        
        vertexBC->Release();
        pixelBC->Release();
        
        initialized = true;
    }
    
    void DrawRect(Game* game, const Rect& rect, const Vector4& color) {
        if (!initialized || !game || !game->Context) return;
        
        struct Vertex {
            Vector3 position;
            Vector4 color;
        };
        
        Vertex vertices[] = {
            { Vector3(rect.Left(),  rect.Top(),    0.0f), color },
            { Vector3(rect.Right(), rect.Top(),    0.0f), color },
            { Vector3(rect.Right(), rect.Bottom(), 0.0f), color },
            { Vector3(rect.Left(),  rect.Bottom(), 0.0f), color }
        };
        
        UINT indices[] = { 0, 1, 2, 0, 2, 3 };
        
        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = sizeof(vertices);
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        
        D3D11_SUBRESOURCE_DATA vertexData = {};
        vertexData.pSysMem = vertices;
        
        ID3D11Buffer* tempVB = nullptr;
        game->Device->CreateBuffer(&vertexDesc, &vertexData, &tempVB);
        
        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(indices);
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        
        D3D11_SUBRESOURCE_DATA indexData = {};
        indexData.pSysMem = indices;
        
        ID3D11Buffer* tempIB = nullptr;
        game->Device->CreateBuffer(&indexDesc, &indexData, &tempIB);
        
        if (!tempVB || !tempIB) {
            if (tempVB) tempVB->Release();
            if (tempIB) tempIB->Release();
            return;
        }
        
        game->Context->IASetInputLayout(inputLayout);
        game->Context->VSSetShader(vertexShader, nullptr, 0);
        game->Context->PSSetShader(pixelShader, nullptr, 0);
        game->Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        game->Context->IASetVertexBuffers(0, 1, &tempVB, &stride, &offset);
        game->Context->IASetIndexBuffer(tempIB, DXGI_FORMAT_R32_UINT, 0);
        
        game->Context->DrawIndexed(6, 0, 0);
        
        tempVB->Release();
        tempIB->Release();
    }
    
    void DestroyResources() {
        if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
        if (vertexShader) { vertexShader->Release(); vertexShader = nullptr; }
        if (pixelShader) { pixelShader->Release(); pixelShader = nullptr; }
        if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
        if (indexBuffer) { indexBuffer->Release(); indexBuffer = nullptr; }
        initialized = false;
    }
};