#include "Game.h"
#include "OrbitalCameraGameComponent.h"
#include "InfiniteGridGameComponent.h"
#include "PropGameComponent.h"
#include "KatamariBallGameComponent.h"
#include <iostream>
#include <random>
#include <cmath>
#include <vector>
#include <string>


float GetModelScale(const std::string& modelPath) {
    if (modelPath.find("childrens_chair") != std::string::npos) return 0.01f;
    if (modelPath.find("BarrelNewOBJ") != std::string::npos) return 0.8f;
    if (modelPath.find("coffee_table") != std::string::npos) return 0.012f;
    if (modelPath.find("diamond") != std::string::npos) return 0.6f;
    if (modelPath.find("hammer") != std::string::npos) return 0.01f;
    if (modelPath.find("knife") != std::string::npos) return 0.04f;
    if (modelPath.find("metal_table") != std::string::npos) return 0.004f;
    if (modelPath.find("obj.obj") != std::string::npos) return 0.8f;
    if (modelPath.find("signboard_02") != std::string::npos) return 0.12f;
    return 0.01f;
}


int main() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    std::cout << "=== KATAMARI GAME ===" << std::endl;
    std::cout << "WASD - Move ball | Mouse - Look | Collect objects!" << std::endl;
    std::cout << "Green wireframe = Ball collider | Red wireframe = Object collider" << std::endl;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    Game game(L"Katamari", hInstance, 800, 800);

    InfiniteGridGameComponent* grid = new InfiniteGridGameComponent(&game);
    game.components.push_back(grid);

    OrbitalCameraGameComponent* camera = new OrbitalCameraGameComponent(&game, Vector3(0, 3, 0), 12.0f);
    game.OrbitalCamera = camera;
    game.Camera = camera;
    camera->Initialize();

    KatamariBallGameComponent* ball = new KatamariBallGameComponent(&game, camera, Vector3(0, 0.5f, 0), 0.6f, "models/marble.jpg");
    game.components.push_back(ball);

    std::vector<std::string> models = {
        "models/childrens_chair/childrens_chair.obj",
        //"models/BarrelNewOBJ/BarrelNewOBJ.obj",
        "models/coffee_table/coffee_table.obj",
        //"models/diamond/Diamond.obj",
        "models/hammer/hammer.obj",
        "models/knife/knife.obj",
        "models/metal_table/metal_table.obj",
        //"models/obj/obj.obj",
        //"models/signboard_02/signboard_02.obj"
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> modelDist(0, (int)models.size() - 1);
    std::uniform_real_distribution<float> scaleDist(0.008f, 0.015f);

    std::vector<Vector3> usedPositions;

    auto isTooClose = [&](const Vector3& pos, float minDist) {
        for (const auto& used : usedPositions) {
            float dx = pos.x - used.x;
            float dz = pos.z - used.z;
            float dist = sqrt(dx * dx + dz * dz);
            if (dist < minDist) return true;
        }
        return false;
        };

    float distances[] = { 9.0f, 11.0f, 13.0f, 10.0f, 12.0f, 14.0f, 8.0f };
    float angles[] = { 0.2f, 1.8f, 3.1f, 4.5f, 5.0f, 2.5f, 4.0f };

    for (int i = 0; i < 10; i++) {
        float distance = distances[i];
        float angle = angles[i];

        float x = cos(angle) * distance;
        float z = sin(angle) * distance;

        Vector3 pos(x, 0, z);

        if (abs(x) < 6.0f && abs(z) < 6.0f) {
            if (x >= 0) x += 4.0f;
            else x -= 4.0f;
            if (z >= 0) z += 4.0f;
            else z -= 4.0f;
            pos = Vector3(x, 0, z);
        }

        usedPositions.push_back(pos);

        int modelIndex = modelDist(gen);
        float scale = scaleDist(gen);

        PropGameComponent* prop = new PropGameComponent(&game, models[modelIndex], pos, GetModelScale(models[modelIndex]));
        game.components.push_back(prop);
        ball->props.push_back(prop);

        size_t lastSlash = models[modelIndex].find_last_of("/");
        std::string modelName = (lastSlash != std::string::npos) ? models[modelIndex].substr(lastSlash + 1) : models[modelIndex];

        std::cout << "[Spawn] Prop " << i << ": " << modelName
            << " at (" << x << ", 0, " << z << ") with scale " << scale << std::endl;
    }

    std::cout << "Total objects: " << game.components.size() << std::endl;
    std::cout << "Ball spawn at (0, 0.5, 0)" << std::endl;
    std::cout << "Props are placed at distance 8-14 units from center" << std::endl;
    std::cout << "Props scale is 0.008-0.015 (very small!)" << std::endl;
    std::cout << "Initializing..." << std::endl;

    HRESULT hr = game.Initialize();
    if (FAILED(hr)) {
        MessageBox(nullptr, L"Failed to initialize", L"Error", MB_OK);
        return 1;
    }

    std::cout << "Game started! Go collect objects!" << std::endl;
    game.Run();

    fclose(f);
    FreeConsole();
    return 0;
}