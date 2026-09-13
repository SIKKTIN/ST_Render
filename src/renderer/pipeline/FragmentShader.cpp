#define NOMINMAX
#include "renderer/pipeline/FragmentShader.hpp"

#include "core/math/Vector4.hpp"
#include <algorithm>
#include <cmath>

namespace ST {
    FragmentShader::FragmentShader()
        : m_ambient(0.1f, 0.1f, 0.1f)
        , m_textureWidth(0)
        , m_textureHeight(0)
        , m_hasTexture(false)
        , m_roughnessTextureWidth(0)
        , m_roughnessTextureHeight(0)
        , m_hasRoughnessTexture(false)
        , m_metallicTextureWidth(0)
        , m_metallicTextureHeight(0)
        , m_hasMetallicTexture(false)
        , m_normalTextureWidth(0)
        , m_normalTextureHeight(0)
        , m_hasNormalTexture(false) {
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

	void FragmentShader::setRoughnessTexture(const std::vector<Color>& texture, int width, int height) {
		m_roughnessTexture = texture;
		m_roughnessTextureWidth = width;
		m_roughnessTextureHeight = height;
		m_hasRoughnessTexture = !texture.empty() && width > 0 && height > 0;
	}

	void FragmentShader::setMetallicTexture(const std::vector<Color>& texture, int width, int height) {
		m_metallicTexture = texture;
		m_metallicTextureWidth = width;
		m_metallicTextureHeight = height;
		m_hasMetallicTexture = !texture.empty() && width > 0 && height > 0;
	}

	void FragmentShader::setNormalTexture(const std::vector<Color>& texture, int width, int height) {
		m_normalTexture = texture;
		m_normalTextureWidth = width;
		m_normalTextureHeight = height;
		m_hasNormalTexture = !texture.empty() && width > 0 && height > 0;
	}

	namespace {
	Color sampleTextureBilinearFrom(const std::vector<Color>& texture, int width, int height,
		const Vector2& uv) {
		if (texture.empty() || width <= 0 || height <= 0) return Color::white();
		float u = std::fmod(uv.x, 1.0f);
		float v = std::fmod(uv.y, 1.0f);
		if (u < 0.0f) u += 1.0f;
		if (v < 0.0f) v += 1.0f;
		const float x = u * static_cast<float>(width) - 0.5f;
		const float y = v * static_cast<float>(height) - 0.5f;
		const int x0 = static_cast<int>(std::floor(x));
		const int y0 = static_cast<int>(std::floor(y));
		const float fx = x - static_cast<float>(x0);
		const float fy = y - static_cast<float>(y0);
		const int xWrapped = ((x0 % width) + width) % width;
		const int yWrapped = ((y0 % height) + height) % height;
		const int x1 = (xWrapped + 1) % width;
		const int y1 = (yWrapped + 1) % height;
		const Color top = Color::lerp(texture[yWrapped * width + xWrapped],
			texture[yWrapped * width + x1], fx);
		const Color bottom = Color::lerp(texture[y1 * width + xWrapped],
			texture[y1 * width + x1], fx);
		return Color::lerp(top, bottom, fy);
	}

	float sampleScalarMap(const std::vector<Color>& texture, int width, int height,
	                     const Vector2& uv, bool enabled) {
		if (!enabled || texture.empty() || width <= 0 || height <= 0) return 1.0f;
		float u = std::fmod(uv.x, 1.0f);
		float v = std::fmod(uv.y, 1.0f);
		if (u < 0.0f) u += 1.0f;
		if (v < 0.0f) v += 1.0f;
		const int x = std::clamp(static_cast<int>(u * width), 0, width - 1);
		const int y = std::clamp(static_cast<int>(v * height), 0, height - 1);
		const Color& sample = texture[y * width + x];
		return (sample.r + sample.g + sample.b) / 3.0f;
	}
}

	Color FragmentShader::sampleTexture(const Vector2& uv) {
		return sampleTextureBilinear(uv);
	}

	Color FragmentShader::sampleTextureBilinear(const Vector2& uv) {
		if (!m_hasTexture) {
			return Color::white();
		}

		float u = std::fmod(uv.x, 1.0f);
		float v = std::fmod(uv.y, 1.0f);
		if (u < 0) u += 1.0f;
		if (v < 0) v += 1.0f;

		const float x = u * static_cast<float>(m_textureWidth) - 0.5f;
		const float y = v * static_cast<float>(m_textureHeight) - 0.5f;
		const int x0 = static_cast<int>(std::floor(x));
		const int y0 = static_cast<int>(std::floor(y));
		const float fx = x - static_cast<float>(x0);
		const float fy = y - static_cast<float>(y0);
		const int wrappedX0 = ((x0 % m_textureWidth) + m_textureWidth) % m_textureWidth;
		const int wrappedY0 = ((y0 % m_textureHeight) + m_textureHeight) % m_textureHeight;
		const int wrappedX1 = (wrappedX0 + 1) % m_textureWidth;
		const int wrappedY1 = (wrappedY0 + 1) % m_textureHeight;

		const Color& c00 = m_texture[wrappedY0 * m_textureWidth + wrappedX0];
		const Color& c10 = m_texture[wrappedY0 * m_textureWidth + wrappedX1];
		const Color& c01 = m_texture[wrappedY1 * m_textureWidth + wrappedX0];
		const Color& c11 = m_texture[wrappedY1 * m_textureWidth + wrappedX1];
		const Color top = Color::lerp(c00, c10, fx);
		const Color bottom = Color::lerp(c01, c11, fx);
		return Color::lerp(top, bottom, fy);
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
			vertexOut.tangent.normalized(),
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
		Vector3 shadingNormal = fragment.normal.normalized();
		if (m_hasNormalTexture) {
			const Color normalSample = sampleTextureBilinearFrom(m_normalTexture, m_normalTextureWidth,
				m_normalTextureHeight, fragment.texCoord);
			const Vector3 tangent = fragment.tangent.normalized();
			const Vector3 bitangent = shadingNormal.cross(tangent).normalized();
			const float normalStrength = std::max(0.0f, m_material.normalStrength);
			const Vector3 tangentNormal((normalSample.r * 2.0f - 1.0f) * normalStrength,
				(normalSample.g * 2.0f - 1.0f) * normalStrength,
				normalSample.b * 2.0f - 1.0f);
			shadingNormal = (tangent * tangentNormal.x + bitangent * tangentNormal.y +
				shadingNormal * tangentNormal.z).normalized();
		}
		Vector3 totalLight = m_ambient * m_material.ambient;
		const float metallicMap = sampleScalarMap(m_metallicTexture, m_metallicTextureWidth,
			m_metallicTextureHeight, fragment.texCoord, m_hasMetallicTexture);
		const float roughnessMap = sampleScalarMap(m_roughnessTexture, m_roughnessTextureWidth,
			m_roughnessTextureHeight, fragment.texCoord, m_hasRoughnessTexture);
		const float metallic = clamp(m_material.metallicFactor * metallicMap, 0.0f, 1.0f);
		const float roughness = clamp(m_material.roughness * roughnessMap, 0.02f, 1.0f);
		const float specularPower = std::max(1.0f,
			(1.0f - roughness) * (1.0f - roughness) * 256.0f);
		const Vector3 specularColor =
			m_material.specular * (1.0f - metallic) + m_material.diffuse * metallic;

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

			float diff = std::max(0.0f, shadingNormal.dot(lightDir));
			Vector3 diffuse = m_material.diffuse * (diff * (1.0f - metallic)) *
				light.color.rgb * light.intensity * attenuation;

			// Blinn-Phong: use half-vector
			Vector3 halfDir = (lightDir + viewDir).normalized();
			float spec = std::pow(std::max(0.0f, shadingNormal.dot(halfDir)), specularPower);
			Vector3 specular = specularColor * spec * light.color.rgb * light.intensity * attenuation;

			totalLight = totalLight + diffuse + specular;
		}

		Color texColor = m_hasTexture ? sampleTexture(fragment.texCoord) : fragment.color;
		Vector3 result = Vector3(totalLight.x * texColor.r,
			totalLight.y * texColor.g,
			totalLight.z * texColor.b);
		result = result + m_material.emission;

		return Color(saturate(result), texColor.a);
	}
}
