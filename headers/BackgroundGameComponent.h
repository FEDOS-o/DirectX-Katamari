#pragma once


#include "GameComponent.h"

class BackgroundGameComponent : public GameComponent {
private:
	float TotalTime;

public:
	BackgroundGameComponent(Game* game) : GameComponent(game), TotalTime(0.0f) {}

	void Update(float deltaTime) override {
		TotalTime += deltaTime;
		while (TotalTime > 1.0f) {
			TotalTime -= 1.0f;
		}
	}

	void Draw() override {
		if (!game || !game->Context || !game->RenderView) {
			return;
		}

		float clearColor[] = { TotalTime, 0.1f, 0.1f, 1.0f };
		game->Context->ClearRenderTargetView(game->RenderView, clearColor);
	}
};
