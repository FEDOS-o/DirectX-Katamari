// GameComponent.h
#pragma once

#include "Game.h"

class Game;
class RenderingSystem;  // Forward declaration

class GameComponent {
protected:
	Game* game;
public:
	GameComponent(Game* game) : game(game) {}

	virtual void Initialize() {};
	virtual void Update(float deltaTime) {};
	virtual void Draw() {};  // Оставляем для forward rendering (прозрачные объекты)
	virtual void DrawGeometry(RenderingSystem* rs) {};  // НОВЫЙ МЕТОД для Deferred Geometry Pass
	virtual void DrawShadow() {};
	virtual void DestroyResources() {};
	virtual void Reload() {}
};