#include "DepthShader.h"

DepthShader::DepthShader() {}

DepthShader::~DepthShader() {}

bool DepthShader::Initialize(ID3D11Device* device, HWND hwnd) {
    return InitializeShader(device, hwnd, L"depth.vs", L"depth.ps");
}

void DepthShader::Shutdown() {
    ShutdownShader();
}

bool DepthShader::Render(ID3D11DeviceContext* context, int indexCount, XMMATRIX world, XMMATRIX view, XMMATRIX projection) {
    if (!SetShaderParameters(context, world, view, projection))
        return false;

    RenderShader(context, indexCount);
    return true;
}

bool DepthShader::InitializeShader(ID3D11Device* device, HWND hwnd, const wchar_t* vsFile, const wchar_t* psFile) {
    HRESULT result;
    ID3D10Blob* errorMessage = nullptr;
    ID3D10Blob* vertexShaderBuffer = nullptr;
    ID3D10Blob* pixelShaderBuffer = nullptr;

    // Компилируем вершинный шейдер из строки (встроенный)
    const char* vsCode = R"(
        cbuffer MatrixBuffer {
            matrix worldMatrix;
            matrix viewMatrix;
            matrix projectionMatrix;
        };

        struct VertexInputType {
            float4 position : POSITION;
        };

        struct PixelInputType {
            float4 position : SV_POSITION;
            float4 depthPosition : TEXTURE0;
        };

        PixelInputType DepthVertexShader(VertexInputType input) {
            PixelInputType output;
            
            input.position.w = 1.0f;
            
            output.position = mul(input.position, worldMatrix);
            output.position = mul(output.position, viewMatrix);
            output.position = mul(output.position, projectionMatrix);
            
            output.depthPosition = output.position;
            
            return output;
        }
    )";

    result = D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "DepthVertexShader", "vs_5_0",
        D3DCOMPILE_DEBUG, 0, &vertexShaderBuffer, &errorMessage);
    if (FAILED(result)) {
        if (errorMessage) OutputShaderErrorMessage(errorMessage, hwnd, L"depth.vs");
        else MessageBox(hwnd, L"Missing depth.vs", L"Error", MB_OK);
        return false;
    }

    // Компилируем пиксельный шейдер
    const char* psCode = R"(
        struct PixelInputType {
            float4 position : SV_POSITION;
            float4 depthPosition : TEXTURE0;
        };

        float4 DepthPixelShader(PixelInputType input) : SV_TARGET {
            float depthValue = input.depthPosition.z / input.depthPosition.w;
            return float4(depthValue, depthValue, depthValue, 1.0f);
        }
    )";

    result = D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "DepthPixelShader", "ps_5_0",
        D3DCOMPILE_DEBUG, 0, &pixelShaderBuffer, &errorMessage);
    if (FAILED(result)) {
        if (errorMessage) OutputShaderErrorMessage(errorMessage, hwnd, L"depth.ps");
        else MessageBox(hwnd, L"Missing depth.ps", L"Error", MB_OK);
        return false;
    }

    // Создаем шейдеры
    device->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), nullptr, &m_vertexShader);
    device->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(), pixelShaderBuffer->GetBufferSize(), nullptr, &m_pixelShader);

    // Input layout (только позиция)
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    device->CreateInputLayout(layout, 1, vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_layout);

    vertexShaderBuffer->Release();
    pixelShaderBuffer->Release();

    // Константный буфер
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.ByteWidth = sizeof(MatrixBufferType);
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    device->CreateBuffer(&bufferDesc, nullptr, &m_matrixBuffer);

    return true;
}

void DepthShader::ShutdownShader() {
    if (m_matrixBuffer) m_matrixBuffer->Release();
    if (m_layout) m_layout->Release();
    if (m_pixelShader) m_pixelShader->Release();
    if (m_vertexShader) m_vertexShader->Release();
}

void DepthShader::OutputShaderErrorMessage(ID3D10Blob* error, HWND hwnd, const wchar_t* file) {
    char* msg = (char*)error->GetBufferPointer();
    std::ofstream fout("shader-error.txt");
    fout << msg;
    fout.close();
    error->Release();
    MessageBox(hwnd, L"Shader compile error. Check shader-error.txt", file, MB_OK);
}

bool DepthShader::SetShaderParameters(ID3D11DeviceContext* context, XMMATRIX world, XMMATRIX view, XMMATRIX projection) {
    D3D11_MAPPED_SUBRESOURCE mapped;

    world = XMMatrixTranspose(world);
    view = XMMatrixTranspose(view);
    projection = XMMatrixTranspose(projection);

    if (FAILED(context->Map(m_matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return false;

    MatrixBufferType* data = (MatrixBufferType*)mapped.pData;
    data->world = world;
    data->view = view;
    data->projection = projection;

    context->Unmap(m_matrixBuffer, 0);
    context->VSSetConstantBuffers(0, 1, &m_matrixBuffer);

    return true;
}

void DepthShader::RenderShader(ID3D11DeviceContext* context, int indexCount) {
    context->IASetInputLayout(m_layout);
    context->VSSetShader(m_vertexShader, nullptr, 0);
    context->PSSetShader(m_pixelShader, nullptr, 0);
    context->DrawIndexed(indexCount, 0, 0);
}