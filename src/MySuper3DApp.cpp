#include "Game.h"
#include "OrbitalCamera.h"
#include "Prop.h"
#include "KatamariBall.h"
#include "TexturedGround.h"
#include "Skybox.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"
#include "Particle.h"
#include <random>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>

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
    game.skybox = skybox;
    game.AddComponent(skybox);

    // ============================================
    // DIRECTIONAL LIGHT - основное освещение
    // ============================================
    DirectionalLightComponent* sunLight = game.GetMainDirectionalLight();
    if (sunLight) {
        sunLight->SetDirection(Vector3(0.5f, -1.0f, 0.3f));
        sunLight->SetColor(Vector4(1.0f, 0.95f, 0.85f, 1.0f));
        sunLight->SetIntensity(1.0f);
    }

    // ============================================
    // POINT LIGHTS - на расстоянии 15-25 единиц от центра
    // ============================================

    PointLightComponent* redLight = new PointLightComponent(&game,
        Vector3(18, 4.0f, 0),
        Vector4(1.0f, 0.1f, 0.1f, 1.0f),
        1.2f,
        20.0f);
    redLight->SetAttenuation(1.0f, 0.04f, 0.01f);
    game.AddLight(redLight);

    PointLightComponent* greenLight = new PointLightComponent(&game,
        Vector3(-18, 4.0f, 0),
        Vector4(0.1f, 1.0f, 0.1f, 1.0f),
        1.2f,
        20.0f);
    greenLight->SetAttenuation(1.0f, 0.04f, 0.01f);
    game.AddLight(greenLight);

    PointLightComponent* blueLight = new PointLightComponent(&game,
        Vector3(0, 4.0f, 18),
        Vector4(0.1f, 0.2f, 1.0f, 1.0f),
        1.2f,
        20.0f);
    blueLight->SetAttenuation(1.0f, 0.04f, 0.01f);
    game.AddLight(blueLight);

    PointLightComponent* yellowLight = new PointLightComponent(&game,
        Vector3(0, 4.0f, -18),
        Vector4(1.0f, 1.0f, 0.1f, 1.0f),
        1.0f,
        20.0f);
    yellowLight->SetAttenuation(1.0f, 0.04f, 0.01f);
    game.AddLight(yellowLight);

    PointLightComponent* magentaLight = new PointLightComponent(&game,
        Vector3(14, 3.0f, 14),
        Vector4(1.0f, 0.1f, 0.8f, 1.0f),
        0.9f,
        18.0f);
    magentaLight->SetAttenuation(1.0f, 0.05f, 0.015f);
    game.AddLight(magentaLight);

    PointLightComponent* cyanLight = new PointLightComponent(&game,
        Vector3(-14, 3.0f, 14),
        Vector4(0.1f, 0.8f, 0.8f, 1.0f),
        0.9f,
        18.0f);
    cyanLight->SetAttenuation(1.0f, 0.05f, 0.015f);
    game.AddLight(cyanLight);

    PointLightComponent* orangeLight = new PointLightComponent(&game,
        Vector3(14, 3.0f, -14),
        Vector4(1.0f, 0.5f, 0.1f, 1.0f),
        0.9f,
        18.0f);
    orangeLight->SetAttenuation(1.0f, 0.05f, 0.015f);
    game.AddLight(orangeLight);

    PointLightComponent* pinkLight = new PointLightComponent(&game,
        Vector3(-14, 3.0f, -14),
        Vector4(1.0f, 0.4f, 0.7f, 1.0f),
        0.9f,
        18.0f);
    pinkLight->SetAttenuation(1.0f, 0.05f, 0.015f);
    game.AddLight(pinkLight);

    // ============================================
    // SPOT LIGHTS
    // ============================================

    SpotLightComponent* redSpot = new SpotLightComponent(&game,
        Vector3(-12, 5, 0),
        Vector3(1, -0.2f, 0),
        Vector4(1.0f, 0.1f, 0.1f, 1.0f),
        1.3f,
        18.0f,
        XM_PI / 5.0f,
        2.5f);
    game.AddLight(redSpot);

    SpotLightComponent* blueSpot = new SpotLightComponent(&game,
        Vector3(12, 5, 0),
        Vector3(-1, -0.2f, 0),
        Vector4(0.1f, 0.2f, 1.0f, 1.0f),
        1.3f,
        18.0f,
        XM_PI / 5.0f,
        2.5f);
    game.AddLight(blueSpot);

    SpotLightComponent* greenSpot = new SpotLightComponent(&game,
        Vector3(0, 10, 0),
        Vector3(0, -1, 0),
        Vector4(0.1f, 1.0f, 0.1f, 1.0f),
        1.0f,
        16.0f,
        XM_PI / 6.0f,
        3.0f);
    game.AddLight(greenSpot);

    SpotLightComponent* yellowSpot = new SpotLightComponent(&game,
        Vector3(0, 7, -12),
        Vector3(0, -0.3f, 1),
        Vector4(1.0f, 1.0f, 0.1f, 1.0f),
        1.1f,
        16.0f,
        XM_PI / 4.0f,
        2.0f);
    game.AddLight(yellowSpot);

    SpotLightComponent* magentaSpot = new SpotLightComponent(&game,
        Vector3(10, 5, -10),
        Vector3(-0.7f, -0.2f, 0.7f),
        Vector4(1.0f, 0.1f, 0.8f, 1.0f),
        1.0f,
        16.0f,
        XM_PI / 4.5f,
        2.5f);
    game.AddLight(magentaSpot);

    SpotLightComponent* cyanSpot = new SpotLightComponent(&game,
        Vector3(-10, 5, 10),
        Vector3(0.7f, -0.2f, -0.7f),
        Vector4(0.1f, 0.8f, 0.8f, 1.0f),
        1.0f,
        16.0f,
        XM_PI / 4.5f,
        2.5f);
    game.AddLight(cyanSpot);

    // ============================================
    // АНИМИРОВАННЫЕ LIGHTS
    // ============================================

    PointLightComponent* movingLight = new PointLightComponent(&game,
        Vector3(8, 3, 0),
        Vector4(1.0f, 0.3f, 0.3f, 1.0f),
        0.8f,
        12.0f);
    movingLight->SetAttenuation(1.0f, 0.07f, 0.02f);
    game.AddLight(movingLight);

    PointLightComponent* movingLight2 = new PointLightComponent(&game,
        Vector3(0, 2, 8),
        Vector4(0.3f, 0.5f, 1.0f, 1.0f),
        0.7f,
        12.0f);
    movingLight2->SetAttenuation(1.0f, 0.07f, 0.02f);
    game.AddLight(movingLight2);

    PointLightComponent* flickerLight = new PointLightComponent(&game,
        Vector3(-8, 2, 6),
        Vector4(1.0f, 1.0f, 1.0f, 1.0f),
        0.6f,
        10.0f);
    flickerLight->SetAttenuation(1.0f, 0.08f, 0.025f);
    game.AddLight(flickerLight);

    TexturedGround* ground = new TexturedGround(&game, 100.0f, 100, "models/wood.jpg");
    game.AddComponent(ground);

    OrbitalCamera* camera = new OrbitalCamera(&game, Vector3(0, 3, 0), 18.0f);
    game.orbitalCamera = camera;
    game.Camera = camera;
    game.AddComponent(camera);
    camera->Initialize();

    KatamariBall* ball = new KatamariBall(&game, camera, Vector3(0, 0.5f, 0), 0.6f, "models/marble.jpg");
    game.AddComponent(ball);

    ParticleEmitter* fountain1 = new ParticleEmitter(&game, Vector3(5.0f, 0.5f, 5.0f));
    fountain1->SetupFountain(Vector3(5.0f, 0.5f, 5.0f), Vector4(1.0f, 0.5f, 0.2f, 1.0f));
    fountain1->UseGBufferCollision(true);
    game.AddComponent(fountain1);

    ParticleEmitter* fountain2 = new ParticleEmitter(&game, Vector3(0.0f, 2.0f, 0.0f));
    fountain2->SetupFountain(Vector3(0.0f, 2.0f, 0.0f), Vector4(0.2f, 0.5f, 1.0f, 1.0f));
    fountain2->UseGBufferCollision(true);
    fountain2->SetRestitution(0.7f);
    game.AddComponent(fountain2);

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

    float distances[] = { 9.0f, 11.0f, 13.0f, 10.0f, 12.0f, 14.0f, 8.0f, 15.0f, 7.0f, 16.0f };
    float angles[] = { 0.2f, 1.8f, 3.1f, 4.5f, 5.0f, 2.5f, 4.0f, 1.0f, 3.8f, 5.5f };

    // Сохраняем указатели на пропы для вывода информации
    std::vector<Prop*> createdProps;

    for (int i = 0; i < 10; i++) {
        float x = cos(angles[i]) * distances[i];
        float z = sin(angles[i]) * distances[i];
        Vector3 pos(x, 0, z);

        int modelIndex = modelDist(gen);
        Prop* prop = new Prop(&game, models[modelIndex], pos, GetModelScale(models[modelIndex]));
        game.AddComponent(prop);
        ball->props.push_back(prop);
        createdProps.push_back(prop);

        std::cout << "Created prop with model: " << models[modelIndex]
            << " at position (" << x << ", 0, " << z << ")"
                << " ID: " << prop->GetId() << std::endl;
    }

    HRESULT hr = game.Initialize();
    if (FAILED(hr)) {
        MessageBox(nullptr, L"Failed to initialize", L"Error", MB_OK);
        return 1;
    }

    std::cout << "\n=== G-Buffer Object ID System Active ===" << std::endl;
    std::cout << "Click left mouse button to pick objects!" << std::endl;
    std::cout << "Press 'C' to switch camera modes" << std::endl;
    std::cout << "Press ESC to exit\n" << std::endl;

    // Переменные для отслеживания состояния кнопки мыши
    static bool leftMousePressed = false;
    static uint32_t lastPickedId = 0;

    // Запускаем игровой цикл (в реальном приложении нужно добавить обработку в Game::Update)
    // В данном примере просто запускаем игру
    game.Run();

    fclose(f);
    FreeConsole();
    return 0;
}