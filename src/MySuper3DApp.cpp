#include "Game.h"
#include "OrbitalCamera.h"
#include "Prop.h"
#include "KatamariBall.h"
#include "TexturedGround.h"
#include "Skybox.h"
#include <random>
#include <cmath>
#include <vector>
#include <string>

float GetModelScale(const std::string& modelPath) {
    if (modelPath.find("childrens_chair") != std::string::npos) return 0.05f;
    if (modelPath.find("BarrelNewOBJ") != std::string::npos) return 0.8f;
    if (modelPath.find("coffee_table") != std::string::npos) return 0.03f;
    if (modelPath.find("diamond") != std::string::npos) return 0.6f;
    if (modelPath.find("hammer") != std::string::npos) return 0.01f;
    if (modelPath.find("knife") != std::string::npos) return 0.04f;
    if (modelPath.find("metal_table") != std::string::npos) return 0.04f;
    if (modelPath.find("obj.obj") != std::string::npos) return 0.8f;
    if (modelPath.find("signboard_02") != std::string::npos) return 0.12f;
    return 0.01f;
}

int main() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    Game game(L"Katamari", hInstance, 800, 800);

    Skybox* skybox = new Skybox(&game, "models/cubemap.png");
    game.components.push_back(skybox);

    game.SunLight.direction = Vector3(0.5f, -1.0f, 0.3f);
    game.SunLight.direction.Normalize();
    game.SunLight.ambient = Vector4(1.5f, 1.5f, 1.5f, 1.0f);
    game.SunLight.diffuse = Vector4(1.0f, 0.9f, 0.7f, 1.0f);
    game.SunLight.specular = Vector4(2.0f, 2.0f, 2.0f, 1.0f);

    TexturedGround* ground = new TexturedGround(&game, 100.0f, 100, "models/wood.jpg");
    game.components.push_back(ground);

    OrbitalCamera* camera = new OrbitalCamera(&game, Vector3(0, 3, 0), 12.0f);
    game.orbitalCamera = camera;
    game.Camera = camera;
    camera->Initialize();

    KatamariBall* ball = new KatamariBall(&game, camera, Vector3(0, 0.5f, 0), 0.6f, "models/marble.jpg");
    game.components.push_back(ball);

    std::vector<std::string> models = {
        "models/childrens_chair/childrens_chair.obj",
        "models/coffee_table/coffee_table.obj",
        "models/hammer/hammer.obj",
        "models/knife/knife.obj",
        "models/metal_table/metal_table.obj",
    };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> modelDist(0, (int)models.size() - 1);
    std::uniform_real_distribution<float> scaleDist(0.008f, 0.015f);

    float distances[] = { 9.0f, 11.0f, 13.0f, 10.0f, 12.0f, 14.0f, 8.0f };
    float angles[] = { 0.2f, 1.8f, 3.1f, 4.5f, 5.0f, 2.5f, 4.0f };

    for (int i = 0; i < 7; i++) {
        float x = cos(angles[i]) * distances[i];
        float z = sin(angles[i]) * distances[i];
        Vector3 pos(x, 0, z);

        int modelIndex = modelDist(gen);
        Prop* prop = new Prop(&game, models[modelIndex], pos, GetModelScale(models[modelIndex]));
        game.components.push_back(prop);
        ball->props.push_back(prop);
    }

    HRESULT hr = game.Initialize();
    if (FAILED(hr)) {
        MessageBox(nullptr, L"Failed to initialize", L"Error", MB_OK);
        return 1;
    }

    game.Run();

    fclose(f);
    FreeConsole();
    return 0;
}