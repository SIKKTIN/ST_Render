#pragma once

#include "renderer/geometry/Vertex.hpp"
#include "core/math/Matrix4x4.hpp"
#include "core/math/Vector4.hpp"

namespace ST {
	struct Uniform {
		Matrix4x4 modelMatrix;
		Matrix4x4 viewMatrix;
		Matrix4x4 projectionMatrix;
		Matrix4x4 viewportMatrix;

		Matrix4x4 getModelMatrix() const {
			return modelMatrix;
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
		Vector2 texCoord;

		VertexOut() : color(0,0,0,1), position(0,0,0,1), worldPosition(0,0,0), normal(0,0,1), texCoord(0,0) {}

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
			out.normal = vertex.normal;
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
}