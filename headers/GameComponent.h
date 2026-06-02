// GameComponent.h
#pragma once

#include "Game.h"
#include <cstdint>

class Game;
class RenderingSystem;

class GameComponent {
protected:
    Game* game;
    uint32_t id;

public:
    GameComponent(Game* game) : game(game), id(0) {}

    virtual ~GameComponent() = default;

    uint32_t GetId() const { return id; }
    void SetId(uint32_t newId) { id = newId; }

    virtual void Initialize() {}
    virtual void Update(float deltaTime) {}
    virtual void Draw() {}
    virtual void DrawGeometry(RenderingSystem* rs) {}
    virtual void DrawShadow() {}
    virtual void DestroyResources() {}
    virtual void Reload() {}

    virtual uint32_t GetGBufferId() const { return id; }
};