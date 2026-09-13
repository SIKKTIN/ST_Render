#pragma once

#include "core/math/Vector3.hpp"
#include "core/math/Vector4.hpp"
#include "core/math/Vector2.hpp"

namespace ST {
	struct Vertex	{
		Vector3 position;
		Vector3 normal;
		Vector3 tangent;
		// Sign of the imported bitangent relative to cross(normal, tangent).
		// Keeping the sign is enough to reconstruct a mirrored UV-safe TBN basis
		// without storing another Vector3 per vertex.
		float tangentSign;
		Vector2 texCoord;
		Color color;

		Vertex() : position(0, 0, 0), normal(0, 0, 1), tangent(1, 0, 0), tangentSign(1.0f), texCoord(0, 0), color(1, 1, 1, 1) {}

		Vertex(const Vector3& pos,
			const Vector3& norm = Vector3::forward(),
			const Vector2& uv = Vector2::zero(),
			const Color& col = Color::white())
			: position(pos), normal(norm), tangent(1, 0, 0), tangentSign(1.0f), texCoord(uv), color(col) {
		}

		static Vertex lerp(const Vertex& a, const Vertex& b, float t) {
			Vertex result(
				a.position + t * (b.position - a.position),
				a.normal + t * (b.normal - a.normal),
				a.texCoord + t * (b.texCoord - a.texCoord),
				a.color + t * (b.color - a.color)
			);
			result.tangent = a.tangent + t * (b.tangent - a.tangent);
			result.tangentSign = a.tangentSign + t * (b.tangentSign - a.tangentSign);
			return result;
		}
	};
}
