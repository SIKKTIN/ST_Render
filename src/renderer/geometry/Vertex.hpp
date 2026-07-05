#pragma once

#include "core/math/Vector3.hpp"
#include "core/math/Vector4.hpp"
#include "core/math/Vector2.hpp"

namespace ST {
	struct Vertex	{
		Vector3 position;
		Vector3 normal;
		Vector2 texCoord;
		Color color;

		Vertex() : position(0, 0, 0), normal(0, 0, 1), texCoord(0, 0), color(1, 1, 1, 1) {}

		Vertex(const Vector3& pos,
			const Vector3& norm = Vector3::forward(),
			const Vector2& uv = Vector2::zero(),
			const Color& col = Color::white())
			: position(pos), normal(norm), texCoord(uv), color(col) {
		}

		static Vertex lerp(const Vertex& a, const Vertex& b, float t) {
			return Vertex(
				a.position + t * (b.position - a.position),
				a.normal + t * (b.normal - a.normal),
				a.texCoord + t * (b.texCoord - a.texCoord),
				a.color + t * (b.color - a.color)
			);
		}
	};
}