#pragma once
#include <SimpleMath.h>

using namespace DirectX::SimpleMath;




struct Rect {
	Vector2 position;
	Vector2 size;

    struct Vertex {
        Vector3 Position;
        Vector4 Color;
    };

	float Left() const {
		return position.x - size.x / 2; 
	}


	float Right() const { 
		return position.x + size.x / 2; 
	}


	float Top() const { 
		return position.y - size.y / 2; 
	}


	float Bottom() const { 
		return position.y + size.y / 2; 
	}

	bool Intersects(const Rect& other) const {
		return !(Right() < other.Left() ||
			Left() > other.Right() ||
			Bottom() < other.Top() ||
			Top() > other.Bottom());
	}
};