#pragma once

#include "Game.h"


class Game;

class GameComponent {
protected:
	Game* game;
public:
	GameComponent(Game* game) : game(game) {}

	virtual void Initialize() {};
	virtual void Update(float deltaTime) {};
	virtual void Draw() {};
	virtual void DestroyResources() {};
	virtual void Reload() {}
};