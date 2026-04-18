#pragma once
#include "ModelTypes.h"
#include "Game.h"

class MeshManager {
public:
    static void CreateBuffers(Game* game, MeshData* meshData) {
        if (!game || !game->Device || !meshData) return;

        D3D11_BUFFER_DESC vertexDesc = {};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = (UINT)(sizeof(Vertex) * meshData->vertices.size());
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData = { meshData->vertices.data() };
        game->Device->CreateBuffer(&vertexDesc, &vertexData, &meshData->vertexBuffer);

        D3D11_BUFFER_DESC indexDesc = {};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = (UINT)(sizeof(UINT) * meshData->indexCount);
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData = { meshData->indices.data() };
        game->Device->CreateBuffer(&indexDesc, &indexData, &meshData->indexBuffer);
    }

    static void DrawMesh(ID3D11DeviceContext* context, const MeshData* meshData) {
        if (!meshData->vertexBuffer || !meshData->indexBuffer) return;

        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &meshData->vertexBuffer, &stride, &offset);
        context->IASetIndexBuffer(meshData->indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        context->DrawIndexed(meshData->indexCount, 0, 0);
    }
};