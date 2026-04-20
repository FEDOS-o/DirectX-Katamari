#pragma once

#include "ModelTypes.h"
#include "MeshManager.h"
#include "MaterialManager.h"

#include "ModelLoader.h"
#include "ModelRenderer.h"

class Model3D {
private:
    Game* game;
    ModelLoader loader;
    ModelRenderer renderer;
    MaterialManager materialManager;

    Vector3 position = { 0, 0, 0 };
    Vector3 rotation = { 0, 0, 0 };
    Vector3 scale = { 1, 1, 1 };
    Matrix worldMatrix;
    bool worldMatrixDirty = true;
    bool isValid = false;

    void UpdateWorldMatrix() {
        worldMatrix = Matrix::CreateScale(scale) *
            Matrix::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z) *
            Matrix::CreateTranslation(position);
        worldMatrixDirty = false;
    }

public:
    Model3D(Game* inGame)
        : game(inGame), loader(inGame), renderer(inGame), materialManager(inGame) {
    }

    bool Load(const std::string& path) {
        Unload();
        renderer.Initialize();

        if (loader.Load(path, materialManager)) {
            isValid = true;
            UpdateWorldMatrix();
            return true;
        }
        return false;
    }

    void Unload() {
        loader.Unload();
        materialManager.Clear();
        isValid = false;
    }

    void Draw() {
        if (!isValid || !game->Camera) return;

        if (worldMatrixDirty) {
            UpdateWorldMatrix();
        }

        // Используем DrawWithShadow вместо Draw
        renderer.DrawWithShadow(
            loader.GetMeshes(),
            materialManager,
            worldMatrix,
            game->Camera->GetViewMatrix(),
            game->Camera->GetProjectionMatrix(),
            game->SunLight,
            game->Camera->GetPosition(),
            game->ShadowMapSRV,           // shadow map
            game->ShadowSampler,           // shadow sampler
            game->GetLightViewMatrix(),    // light view
            game->GetLightProjectionMatrix() // light projection
        );
    }

    void SetPosition(const Vector3& pos) { position = pos; worldMatrixDirty = true; }
    void SetRotation(const Vector3& rot) { rotation = rot; worldMatrixDirty = true; }
    void SetScale(const Vector3& scl) { scale = scl; worldMatrixDirty = true; }

    Vector3 GetPosition() const { return position; }
    Vector3 GetRotation() const { return rotation; }
    Vector3 GetScale() const { return scale; }

    bool IsValid() const { return isValid; }
    size_t GetMeshCount() const { return loader.GetMeshes().size(); }
    const std::vector<MeshData*>& GetMeshes() const { return loader.GetMeshes(); }

    void CalculateLocalBoundingBox() { loader.CalculateLocalBoundingBox(); }
    DirectX::BoundingBox GetLocalBoundingBox() const { return loader.GetLocalBoundingBox(); }
};