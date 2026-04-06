#include "Game.h"
#include "OrbitalCameraGameComponent.h"
#include "InfiniteGridGameComponent.h"
#include "PropGameComponent.h"
#include "KatamariBallGameComponent.h"
#include <iostream>
#include <random>

int main() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    std::cout << "=== KATAMARI GAME ===" << std::endl;
    std::cout << "WASD - Move ball | Mouse - Look | Collect objects!" << std::endl;
    std::cout << "Green wireframe = Ball collider | Red wireframe = Object collider" << std::endl;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    Game game(L"Katamari", hInstance, 800, 800);

    // Grid
    InfiniteGridGameComponent* grid = new InfiniteGridGameComponent(&game);
    game.components.push_back(grid);

    // Camera
    OrbitalCameraGameComponent* camera = new OrbitalCameraGameComponent(&game, Vector3(0, 3, 0), 12.0f);
    game.OrbitalCamera = camera;
    game.Camera = camera;
    camera->Initialize();

    // Ball
    KatamariBallGameComponent* ball = new KatamariBallGameComponent(&game, camera, Vector3(0, 0.5f, 0), 6.0f);
    game.components.push_back(ball);

    // Random objects
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);

    for (int i = 0; i < 40; i++) {
        float x = posDist(gen);
        float z = posDist(gen);
        PropGameComponent* prop = new PropGameComponent(&game, "models/childrens_chair/childrens_chair.obj", Vector3(x, 0, z));
        game.components.push_back(prop);
    }

    // Objects near start
    for (float x = -2; x <= 2; x++) {
        for (float z = -2; z <= 2; z++) {
            if (x == 0 && z == 0) continue;
            PropGameComponent* prop = new PropGameComponent(&game, "models/childrens_chair/childrens_chair.obj", Vector3(x * 1.2f, 0, z * 1.2f));
            game.components.push_back(prop);
        }
    }

    std::cout << "Total objects: " << game.components.size() << std::endl;
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