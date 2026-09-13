#pragma once

#include "renderer/geometry/Vertex.hpp"
#include "core/math/Matrix4x4.hpp"
#include "core/math/Vector4.hpp"
#include <array>
#include <algorithm>
#include <cmath>

namespace ST {
	enum class VaryingInterpolation {
		Smooth,
		NoPerspective,
		Flat
	};

	static constexpr size_t MAX_VARYINGS = 16;

	struct Uniform {
		Matrix4x4 modelMatrix;
		Matrix4x4 normalMatrix;
		Matrix4x4 viewMatrix;
		Matrix4x4 projectionMatrix;
		Matrix4x4 viewportMatrix;

		Matrix4x4 getModelMatrix() const {
			return modelMatrix;
		}

		Matrix4x4 getNormalMatrix() const {
			return normalMatrix;
		}

		Matrix4x4 getModelViewMatrix() const {
			return viewMatrix * modelMatrix;
		}

		Matrix4x4 getModelViewProjectionMatrix() const {
			return projectionMatrix * viewMatrix * modelMatrix;
		}

		Matrix4x4 getMvpMatrix() const {
			return projectionMatrix * viewMatrix * modelMatrix;
		}
	};

	struct VertexOut {
		Color color;
		Vector4 position;
		Vector3 worldPosition;
		Vector3 normal;
		Vector3 tangent;
		Vector2 texCoord;
		std::array<Vector4, MAX_VARYINGS> varyings{};
		std::array<VaryingInterpolation, MAX_VARYINGS> varyingInterpolation{};
		int varyingCount = 0;

		VertexOut() : color(0,0,0,1), position(0,0,0,1), worldPosition(0,0,0), normal(0,0,1), tangent(1,0,0), texCoord(0,0) {
			varyingInterpolation.fill(VaryingInterpolation::Smooth);
		}

		void setVarying(size_t index, const Vector4& value,
			VaryingInterpolation interpolation = VaryingInterpolation::Smooth) {
			if (index >= MAX_VARYINGS) return;
			varyings[index] = value;
			varyingInterpolation[index] = interpolation;
			if (static_cast<int>(index + 1) > varyingCount) {
				varyingCount = static_cast<int>(index + 1);
			}
		}

		const Vector4& getVarying(size_t index) const {
			static const Vector4 zero(0, 0, 0, 0);
			return index < MAX_VARYINGS ? varyings[index] : zero;
		}

		Vector3 toNDC() const {
			float invW = 1.0f / position.w;
			return Vector3(position.x * invW, position.y * invW, position.z * invW);
		}
	};

	class VertexShader {
	public:
		Uniform uniforms;

		VertexShader() {}

		explicit VertexShader(const Uniform& uni) : uniforms(uni) {}

		void setUniform(const Uniform& uni) {
			uniforms = uni;
		}

		static VertexOut transformVertex(const Vertex& vertex, const Uniform& uni) {
			VertexOut out;
			out.color = vertex.color;
			out.position = uni.getMvpMatrix() * Vector4(vertex.position, 1.0f);
			out.worldPosition = (uni.getModelMatrix() * Vector4(vertex.position, 1.0f)).toVector3();
			out.normal = uni.getNormalMatrix().transformDirection(vertex.normal).normalized();
			out.tangent = uni.getNormalMatrix().transformDirection(vertex.tangent).normalized();
			out.texCoord = vertex.texCoord;
			return out;
		}

		VertexOut process(const Vertex& vertex) {
			return transformVertex(vertex, uniforms);
		}

		bool isInFrustum(const VertexOut& vertexOut) {
			float w = vertexOut.position.w;
			return (
				vertexOut.position.x >= -w && vertexOut.position.x <= w &&
				vertexOut.position.y >= -w && vertexOut.position.y <= w &&
				vertexOut.position.z >= -w && vertexOut.position.z <= w
				);
		}
	};

	// Linear interpolation of all per-vertex attributes (position in clip
	// space, world position, normal, texcoord, color). Used by the
	// near-plane clipper to construct new vertices on a clipped edge.
	inline VertexOut lerpVertexOut(const VertexOut& a, const VertexOut& b, float t) {
		VertexOut r;
		r.position       = a.position       + (b.position       - a.position)       * t;
		r.worldPosition  = a.worldPosition  + (b.worldPosition  - a.worldPosition)  * t;
		r.normal         = a.normal         + (b.normal         - a.normal)         * t;
		r.tangent       = a.tangent       + (b.tangent       - a.tangent)       * t;
		r.texCoord       = a.texCoord       + (b.texCoord       - a.texCoord)       * t;
		r.color.r        = a.color.r        + (b.color.r        - a.color.r)        * t;
		r.color.g        = a.color.g        + (b.color.g        - a.color.g)        * t;
		r.color.b        = a.color.b        + (b.color.b        - a.color.b)        * t;
		r.color.a        = a.color.a        + (b.color.a        - a.color.a)        * t;
		r.varyingCount   = std::max(a.varyingCount, b.varyingCount);
		for (size_t i = 0; i < MAX_VARYINGS; ++i) {
			r.varyingInterpolation[i] = a.varyingInterpolation[i];
			if (a.varyingInterpolation[i] == VaryingInterpolation::Flat) {
				r.varyings[i] = a.varyings[i];
			} else {
				r.varyings[i] = a.varyings[i] + (b.varyings[i] - a.varyings[i]) * t;
			}
		}
		return r;
	}

	// Sutherland-Hodgman clip against the near plane. The renderer uses
	// the GL convention w_clip = -z_view, so the near plane in clip space
	// is the line w = 0. A vertex with w > 0 is on the inside (visible)
	// side of the near plane; one with w <= 0 is behind the eye.
	//
	// Returns 0 (fully outside, dropped), 1 (fully inside, unchanged), 2
	// (straddling -- emitted as 1 or 2 sub-triangles). The output array
	// `emitVertices` holds the emitted vertices in order; `emitCount` (an
	// out parameter) reports how many of them are valid (3 or 4). Caller
	// tessellates a fan over the emitted vertices: 3 -> one triangle,
	// 4 -> two triangles (0-1-2 and 0-2-3).
	inline void clipTriangleAgainstNearPlane(const VertexOut& v0,
	                                         const VertexOut& v1,
	                                         const VertexOut& v2,
	                                         VertexOut emitVertices[4],
	                                         int& emitCount) {
		const VertexOut* inV[3] = { &v0, &v1, &v2 };
		bool inside[3] = {
			inV[0]->position.w > 0.0f,
			inV[1]->position.w > 0.0f,
			inV[2]->position.w > 0.0f
		};
		int insideCount = (int)inside[0] + (int)inside[1] + (int)inside[2];
		emitCount = 0;

		if (insideCount == 0) {
			return;
		}
		if (insideCount == 3) {
			emitVertices[0] = v0;
			emitVertices[1] = v1;
			emitVertices[2] = v2;
			emitCount = 3;
			return;
		}

		// Walk the three edges (inV[i], inV[i+1]) in order, emitting the
		// clipped points as Sutherland-Hodgman prescribes.
		auto step = [&](const VertexOut& a, const VertexOut& b,
		                bool aInside, bool bInside) {
			if (aInside && bInside) {
				emitVertices[emitCount++] = b;
			} else if (aInside && !bInside) {
				// a inside, b outside: emit intersection only.
				float t = a.position.w / (a.position.w - b.position.w);
				emitVertices[emitCount++] = lerpVertexOut(a, b, t);
			} else if (!aInside && bInside) {
				// a outside, b inside: emit intersection then b.
				float t = a.position.w / (a.position.w - b.position.w);
				emitVertices[emitCount++] = lerpVertexOut(a, b, t);
				emitVertices[emitCount++] = b;
			}
			// else both outside: emit nothing.
		};

		step(*inV[0], *inV[1], inside[0], inside[1]);
		step(*inV[1], *inV[2], inside[1], inside[2]);
		step(*inV[2], *inV[0], inside[2], inside[0]);

		// emitCount is 3 (2in+1out -> triangle) or 4 (1in+2out -> quad).
	}

	// Clip a triangle against the complete homogeneous clip volume.
	// The six planes are x+w >= 0, w-x >= 0, y+w >= 0, w-y >= 0,
	// z+w >= 0 and w-z >= 0.  Clipping in clip space keeps every emitted
	// vertex safe for perspective division and prevents the rasterizer's
	// screen-space bounding box from exploding when the camera is inside a
	// mesh or very close to a surface.
	inline void clipTriangleAgainstFrustum(const VertexOut& v0,
	                                       const VertexOut& v1,
	                                       const VertexOut& v2,
	                                       VertexOut emitVertices[16],
	                                       int& emitCount) {
		VertexOut input[16];
		VertexOut output[16];
		int inputCount = 3;
		input[0] = v0;
		input[1] = v1;
		input[2] = v2;

		auto planeDistance = [](const VertexOut& v, int plane) {
			switch (plane) {
			case 0: return v.position.x + v.position.w; // left
			case 1: return v.position.w - v.position.x; // right
			case 2: return v.position.y + v.position.w; // bottom
			case 3: return v.position.w - v.position.y; // top
			case 4: return v.position.z + v.position.w; // near
			default: return v.position.w - v.position.z; // far
			}
		};

		for (int plane = 0; plane < 6 && inputCount > 0; ++plane) {
			int outputCount = 0;
			const VertexOut* previous = &input[inputCount - 1];
			float previousDistance = planeDistance(*previous, plane);
			bool previousInside = previousDistance >= 0.0f;

			for (int i = 0; i < inputCount; ++i) {
				const VertexOut* current = &input[i];
				float currentDistance = planeDistance(*current, plane);
				bool currentInside = currentDistance >= 0.0f;

				if (previousInside != currentInside && outputCount < 16) {
					float denominator = previousDistance - currentDistance;
					float t = (std::fabs(denominator) > 1e-12f)
						? previousDistance / denominator : 0.0f;
					output[outputCount++] = lerpVertexOut(*previous, *current, t);
				}
				if (currentInside && outputCount < 16) {
					output[outputCount++] = *current;
				}

				previous = current;
				previousDistance = currentDistance;
				previousInside = currentInside;
			}

			for (int i = 0; i < outputCount; ++i) input[i] = output[i];
			inputCount = outputCount;
		}

		emitCount = inputCount;
		for (int i = 0; i < emitCount; ++i) emitVertices[i] = input[i];
	}
}
