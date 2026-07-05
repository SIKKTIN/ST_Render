#pragma once

#define NOMINMAX

#include "Vector3.hpp"
#include "Vector2.hpp"
#include "VertexShader.hpp"
#include <vector>
#include <memory>
#include <algorithm>

namespace ST {
	struct Light {
		Vector3 position;
		Vector3 direction;
		Color color;
		float intensity;
		float attenuation;

		enum class Type {
			POINT,
			DIRECTIONAL,
			SPOT
		} type;

		static Light directional(const Vector3& dir, const Color& col, float intensity) {
			Light light;
			light.type = Type::DIRECTIONAL;
			light.direction = dir.normalized();
			light.color = col;
			light.intensity = intensity;
			return light;
		}

		static Light point(const Vector3& pos, const Color& col, float intensity, float attenuation) {
			Light light;
			light.type = Type::POINT;
			light.position = pos;
			light.color = col;
			light.intensity = intensity;
			light.attenuation = attenuation;
			return light;
		}
	};

	struct Material {
		Vector3 ambient;
		Vector3 diffuse;
		Vector3 specular;
		float shininess;

		static Material defaultMaterial() {
			Material mat;
			mat.ambient = Vector3(0.1f, 0.1f, 0.1f);
			mat.diffuse = Vector3(0.8f, 0.8f, 0.8f);
			mat.specular = Vector3(0.5f, 0.5f, 0.5f);
			mat.shininess = 32.0f;
			return mat;
		}

		static Material metallic(const Vector3& color, float roughness) {
			Material mat;
			mat.ambient = color * 0.1f;
			mat.diffuse = color * 0.6f;
			mat.specular = Vector3(1.0f, 1.0f, 1.0f);
			mat.shininess = 128.0f * (1.0f - roughness);
			return mat;
		}
	};

	struct Fragment {
		Vector3 worldPosition;
		Vector3 normal;
		Vector2 texCoord;
		Color color;
	};

	class FragmentShader {
	public:
		FragmentShader();

		void setMaterial(const Material& mat);
		void setLight(const Light& light);
		void addLight(const Light& light);
		void clearLights();

		void setAmbient(const Vector3& ambient);

		Color shade(const VertexOut& vertexOut);

		Color shadeFlat(const VertexOut& v0, const VertexOut& v1, const VertexOut& v2);
		Color shadeGouraud(const VertexOut& vertexOut);
		Color shadePhong(const Fragment& fragment);
		Color shadeBlinnPhong(const Fragment& fragment);

		void setTexture(const std::vector<Color>& texture, int width, int height);
		Color sampleTexture(const Vector2& uv);
		Color sampleTextureClamp(const Vector2& uv);
	private:
		Material m_material;
		std::vector<Light> m_lights;
		Vector3 m_ambient;
		Vector3 m_viewPosition;

		std::vector<Color> m_texture;
		int m_textureWidth;
		int m_textureHeight;
		bool m_hasTexture;

		Color lerpColor(const Color& a, const Color& b, float t);
		Vector3 lerpVector3(const Vector3& a, const Vector3& b, float t);
		float saturate(float value);
		Vector3 saturate(const Vector3& vec);
	};
}