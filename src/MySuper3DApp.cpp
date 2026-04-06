#include "Game.h"
#include "OrbitalCameraGameComponent.h"
#include "InfiniteGridGameComponent.h"
#include "PropGameComponent.h"
#include "KatamariBallGameComponent.h"
#include <iostream>
#include <random>
#include <cmath>

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
    KatamariBallGameComponent* ball = new KatamariBallGameComponent(&game, camera, Vector3(0, 0.5f, 0), 0.6f, "models/marble.jpg");
    game.components.push_back(ball);

    // Random objects - 7 объектов, разбросанных подальше
    // Зона спавна шара: x = -5..5, z = -5..5
    // Размещаем объекты на расстоянии 8-15 единиц от центра

    std::random_device rd;
    std::mt19937 gen(rd());

    // Позиции для объектов, чтобы они не пересекались друг с другом
    std::vector<Vector3> usedPositions;

    // Функция проверки расстояния между объектами
    auto isTooClose = [&](const Vector3& pos, float minDist) {
        for (const auto& used : usedPositions) {
            float dx = pos.x - used.x;
            float dz = pos.z - used.z;
            float dist = sqrt(dx * dx + dz * dz);
            if (dist < minDist) return true;
        }
        return false;
        };

    // 7 объектов на разных дистанциях и углах
    float distances[] = { 9.0f, 11.0f, 13.0f, 10.0f, 12.0f, 14.0f, 8.0f };
    float angles[] = { 0.2f, 1.8f, 3.1f, 4.5f, 5.0f, 2.5f, 4.0f };

    for (int i = 0; i < 7; i++) {
        float distance = distances[i];
        float angle = angles[i];

        float x = cos(angle) * distance;
        float z = sin(angle) * distance;

        Vector3 pos(x, 0, z);

        // Дополнительная проверка, чтобы объекты не были слишком близко к центру
        if (abs(x) < 6.0f && abs(z) < 6.0f) {
            // Если слишком близко, отодвигаем
            if (x >= 0) x += 4.0f;
            else x -= 4.0f;
            if (z >= 0) z += 4.0f;
            else z -= 4.0f;
            pos = Vector3(x, 0, z);
        }

        usedPositions.push_back(pos);

        PropGameComponent* prop = new PropGameComponent(&game, "models/childrens_chair/childrens_chair.obj", pos);
        game.components.push_back(prop);
        ball->props.push_back(prop);

        std::cout << "[Spawn] Prop " << i << " at (" << x << ", 0, " << z << ")" << std::endl;
    }

    std::cout << "Total objects: " << game.components.size() << std::endl;
    std::cout << "Ball spawn at (0, 0.5, 0)" << std::endl;
    std::cout << "Props are placed at distance 8-14 units from center" << std::endl;
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