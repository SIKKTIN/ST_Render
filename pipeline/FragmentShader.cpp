#define NOMINMAX
#include "FragmentShader.hpp"

#include "Vector4.hpp"
#include <algorithm>
#include <cmath>

namespace ST {
    FragmentShader::FragmentShader()
        : m_ambient(0.1f, 0.1f, 0.1f)
        , m_textureWidth(0)
        , m_textureHeight(0)
        , m_hasTexture(false) {
        m_material = Material::defaultMaterial();
        m_viewPosition = Vector3(0, 0, 5);
    }

	void FragmentShader::setMaterial(const Material& mat) {
		m_material = mat;
	}

	void FragmentShader::setLight(const Light& light) {
		m_lights.clear();
		m_lights.push_back(light);
	}
    
	void FragmentShader::addLight(const Light& light) {
		m_lights.push_back(light);
	}

	void FragmentShader::clearLights() {
		m_lights.clear();
	}

	void FragmentShader::setAmbient(const Vector3& ambient) {
		m_ambient = ambient;
	}

	void FragmentShader::setTexture(const std::vector<Color>& texture, int width, int height) {
		m_texture = texture;
		m_textureWidth = width;
		m_textureHeight = height;
		m_hasTexture = !texture.empty();
	}

	Color FragmentShader::sampleTexture(const Vector2& uv) {
		if (!m_hasTexture) {
			return Color::white();
		}

		float u = std::fmod(uv.x, 1.0f);
		float v = std::fmod(uv.y, 1.0f);
		if (u < 0) u += 1.0f;
		if (v < 0) v += 1.0f;

		int x = static_cast<int>(u * m_textureWidth) % m_textureWidth;
		int y = static_cast<int>(v * m_textureHeight) % m_textureHeight;
		return m_texture[y * m_textureWidth + x];
	}

	Color FragmentShader::sampleTextureClamp(const Vector2& uv) {
		if (!m_hasTexture) return Color::white();

		float u = clamp(uv.x, 0.0f, 1.0f);
		float v = clamp(uv.y, 0.0f, 1.0f);

		int x = static_cast<int>(u * (m_textureWidth - 1));
		int y = static_cast<int>(v * (m_textureHeight - 1));

		return m_texture[y * m_textureWidth + x];
	}

	Color FragmentShader::lerpColor(const Color& a, const Color& b, float t) {
		return Color(
			a.r + (b.r - a.r) * t,
			a.g + (b.g - a.g) * t,
			a.b + (b.b - a.b) * t,
			a.a + (b.a - a.a) * t
		);
	}

	Vector3 FragmentShader::lerpVector3(const Vector3& a, const Vector3& b, float t) {
		return a + (b - a) * t;
	}

	float FragmentShader::saturate(float value) {
		return clamp(value, 0.0f, 1.0f);
	}

	Vector3 FragmentShader::saturate(const Vector3& vec) {
		return Vector3(
			saturate(vec.x),
			saturate(vec.y),
			saturate(vec.z)
		);
	}
	
	Color FragmentShader::shade(const VertexOut& vertexOut) {
		return shadeBlinnPhong({
			vertexOut.worldPosition,
			vertexOut.normal.normalized(),
			vertexOut.texCoord,
			vertexOut.color
			});
	}

	// Flat Shading - Uses the first vertex normal for the entire triangle
	Color FragmentShader::shadeFlat(const VertexOut& v0, const VertexOut& v1, const VertexOut& v2) {
		Vector3 flatNormal = v0.normal.normalized();

		Vector3 viewDir = (m_viewPosition - v0.worldPosition).normalized();
		Vector3 totalLight = m_ambient * m_material.ambient;

		for (const auto& light : m_lights) {
			Vector3 lightDir;
			float attenuation = 1.0f;

			if (light.type == Light::Type::DIRECTIONAL) {
				lightDir = -light.direction;
			}
			else {
				lightDir = (light.position - v0.worldPosition).normalized();
				float distance = (light.position - v0.worldPosition).length();
				attenuation = 1.0f / (1.0f + light.attenuation * distance * distance);
			}

			float diff = std::max(0.0f, flatNormal.dot(lightDir));
			Vector3 diffuse = m_material.diffuse * diff * light.color.rgb * light.intensity * attenuation;

			Vector3 reflectDir = (2.0f * flatNormal.dot(lightDir) * flatNormal - lightDir).normalized();
			float spec = std::pow(std::max(0.0f, viewDir.dot(reflectDir)), m_material.shininess);
			Vector3 specular = m_material.specular * spec * light.color.rgb * light.intensity * attenuation;

			totalLight = totalLight + diffuse + specular;
		}

		return Color(saturate(totalLight), 1.0f);
	}


	Color FragmentShader::shadeGouraud(const VertexOut& vertexOut) {
		return vertexOut.color;
	}

    // Phong Shading - Interpolates vertex normals for per-fragment lighting
    Color FragmentShader::shadePhong(const Fragment& fragment) {
        Vector3 viewDir = (m_viewPosition - fragment.worldPosition).normalized();
        Vector3 totalLight = m_ambient * m_material.ambient;

        for (const auto& light : m_lights) {
            Vector3 lightDir;
            float attenuation = 1.0f;

            if (light.type == Light::Type::DIRECTIONAL) {
                lightDir = -light.direction;
            }
            else {
                lightDir = (light.position - fragment.worldPosition).normalized();
                float distance = (light.position - fragment.worldPosition).length();
                attenuation = 1.0f / (1.0f + light.attenuation * distance * distance);
            }

            float diff = std::max(0.0f, fragment.normal.dot(lightDir));
			
			Vector3 diffuse = m_material.diffuse * (diff * light.color.rgb) * light.intensity * attenuation;

            Vector3 reflectDir = (2.0f * fragment.normal.dot(lightDir) * fragment.normal - lightDir).normalized();
            float spec = std::pow(std::max(0.0f, viewDir.dot(reflectDir)), m_material.shininess);
            Vector3 specular = m_material.specular * spec * light.color.rgb * light.intensity * attenuation;

            totalLight = totalLight + diffuse + specular;
        }

        Color texColor = m_hasTexture ? sampleTexture(fragment.texCoord) : fragment.color;
        Vector3 result = Vector3(totalLight.x * texColor.r,
            totalLight.y * texColor.g,
            totalLight.z * texColor.b);

        return Color(saturate(result), texColor.a);
    }

	// Blinn-Phong - Uses half-vector instead of reflection vector
	Color FragmentShader::shadeBlinnPhong(const Fragment& fragment) {
		Vector3 viewDir = (m_viewPosition - fragment.worldPosition).normalized();
		Vector3 totalLight = m_ambient * m_material.ambient;

		for (const auto& light : m_lights) {
			Vector3 lightDir;
			float attenuation = 1.0f;

			if (light.type == Light::Type::DIRECTIONAL) {
				lightDir = -light.direction;
			}
			else {
				lightDir = (light.position - fragment.worldPosition).normalized();
				float distance = (light.position - fragment.worldPosition).length();
				attenuation = 1.0f / (1.0f + light.attenuation * distance * distance);
			}

			float diff = std::max(0.0f, fragment.normal.dot(lightDir));
			Vector3 diffuse = m_material.diffuse * diff * light.color.rgb * light.intensity * attenuation;

			// Blinn-Phong: use half-vector
			Vector3 halfDir = (lightDir + viewDir).normalized();
			float spec = std::pow(std::max(0.0f, fragment.normal.dot(halfDir)), m_material.shininess);
			Vector3 specular = m_material.specular * spec * light.color.rgb * light.intensity * attenuation;

			totalLight = totalLight + diffuse + specular;
		}

		Color texColor = m_hasTexture ? sampleTexture(fragment.texCoord) : fragment.color;
		Vector3 result = Vector3(totalLight.x * texColor.r,
			totalLight.y * texColor.g,
			totalLight.z * texColor.b);

		return Color(saturate(result), texColor.a);
	}
}


