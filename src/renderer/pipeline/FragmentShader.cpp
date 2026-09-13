#define NOMINMAX
#include "renderer/pipeline/FragmentShader.hpp"

#include "core/math/Vector4.hpp"
#include <algorithm>
#include <cmath>

namespace ST {
namespace {
Color sampleTextureBilinearFrom(const std::vector<Color>& texture, int width, int height,
                                const Vector2& uv);

float srgbChannelToLinear(float value) {
	value = std::clamp(value, 0.0f, 1.0f);
	return value <= 0.04045f
		? value / 12.92f
		: std::pow((value + 0.055f) / 1.055f, 2.4f);
}

Color srgbToLinear(const Color& color) {
	return Color(srgbChannelToLinear(color.r),
		srgbChannelToLinear(color.g),
		srgbChannelToLinear(color.b), color.a);
}
}

    FragmentShader::FragmentShader()
        : m_texture(nullptr)
        , m_ambient(0.1f, 0.1f, 0.1f)
        , m_textureWidth(0)
        , m_textureHeight(0)
        , m_hasTexture(false)
        , m_roughnessTexture(nullptr)
        , m_roughnessTextureWidth(0)
        , m_roughnessTextureHeight(0)
        , m_hasRoughnessTexture(false)
        , m_metallicTexture(nullptr)
        , m_metallicTextureWidth(0)
        , m_metallicTextureHeight(0)
        , m_hasMetallicTexture(false)
        , m_normalTexture(nullptr)
        , m_normalTextureWidth(0)
        , m_normalTextureHeight(0)
        , m_hasNormalTexture(false) {
        m_material = Material::defaultMaterial();
        m_viewPosition = Vector3(0, 0, 5);
		m_environmentColor = Vector3(0.16f, 0.2f, 0.28f);
		m_environmentIntensity = 0.35f;
		m_toneMappingEnabled = true;
		m_exposure = 1.0f;
		m_reducedQuality = false;
		m_environmentTexture = nullptr;
		m_environmentTextureWidth = 0;
		m_environmentTextureHeight = 0;
		m_hasEnvironmentTexture = false;
		m_filteredEnvironmentWidths[0] = 256;
		m_filteredEnvironmentWidths[1] = 128;
		m_filteredEnvironmentWidths[2] = 64;
		m_filteredEnvironmentWidths[3] = 32;
		m_filteredEnvironmentWidths[4] = 16;
		m_filteredEnvironmentWidths[5] = 8;
		m_filteredEnvironmentHeights[0] = 128;
		m_filteredEnvironmentHeights[1] = 64;
		m_filteredEnvironmentHeights[2] = 32;
		m_filteredEnvironmentHeights[3] = 16;
		m_filteredEnvironmentHeights[4] = 8;
		m_filteredEnvironmentHeights[5] = 4;
		for (Vector3& axis : m_environmentDiffuseAxes) axis = m_environmentColor;
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

	void FragmentShader::setEnvironment(const Vector3& color, float intensity) {
		m_environmentColor = color;
		m_environmentIntensity = std::max(0.0f, intensity);
	}

	void FragmentShader::setToneMapping(bool enabled, float exposure) {
		m_toneMappingEnabled = enabled;
		m_exposure = std::max(0.0f, exposure);
	}

	void FragmentShader::setReducedQuality(bool reduced) {
		m_reducedQuality = reduced;
	}

	void FragmentShader::setEnvironmentTexture(const std::vector<Color>& texture, int width, int height) {
		const std::vector<Color>* source = texture.empty() ? nullptr : &texture;
		if (source == m_environmentTexture && width == m_environmentTextureWidth &&
			height == m_environmentTextureHeight) {
			return;
		}
		m_environmentTexture = source;
		m_environmentTextureWidth = width;
		m_environmentTextureHeight = height;
		m_hasEnvironmentTexture = m_environmentTexture != nullptr && width > 0 && height > 0;
		m_filteredEnvironmentLevels.clear();
		if (!m_hasEnvironmentTexture) return;

		// The source environment is a high-frequency tonemapped panorama. Build
		// a small box-filtered mip chain once when the map changes. Roughness can
		// then select a blur level with one lookup per fragment instead of doing
		// several expensive panorama samples for every pixel.
		m_filteredEnvironmentLevels.reserve(EnvironmentLevelCount);
		for (int level = 0; level < EnvironmentLevelCount; ++level) {
			const std::vector<Color>* source = level == 0
				? m_environmentTexture
				: &m_filteredEnvironmentLevels[level - 1];
			const int sourceWidth = level == 0 ? width : m_filteredEnvironmentWidths[level - 1];
			const int sourceHeight = level == 0 ? height : m_filteredEnvironmentHeights[level - 1];
			const int outputWidth = m_filteredEnvironmentWidths[level];
			const int outputHeight = m_filteredEnvironmentHeights[level];
			auto& output = m_filteredEnvironmentLevels.emplace_back(
				static_cast<size_t>(outputWidth) * outputHeight);
			for (int y = 0; y < outputHeight; ++y) {
				for (int x = 0; x < outputWidth; ++x) {
					const float u = (static_cast<float>(x) + 0.5f) /
						static_cast<float>(outputWidth);
					const float v = (static_cast<float>(y) + 0.5f) /
						static_cast<float>(outputHeight);
					const float du = 0.35f / static_cast<float>(outputWidth);
					const float dv = 0.35f / static_cast<float>(outputHeight);
					const auto sampleEnvironmentSource = [&](const Vector2& uv) {
						const Color sample = sampleTextureBilinearFrom(
							*source, sourceWidth, sourceHeight, uv);
						// JPG/PNG panorama pixels are display encoded. Convert the first
						// downsample level to linear light before filtering and PBR use.
						return level == 0 ? srgbToLinear(sample) : sample;
					};
					Color filtered = sampleEnvironmentSource(Vector2(u - du, v - dv));
					filtered += sampleEnvironmentSource(Vector2(u + du, v - dv));
					filtered += sampleEnvironmentSource(Vector2(u - du, v + dv));
					filtered += sampleEnvironmentSource(Vector2(u + du, v + dv));
					output[static_cast<size_t>(y) * outputWidth + x] = filtered * 0.25f;
				}
			}
		}

		// Approximate diffuse irradiance once per environment change. Six axis
		// samples retain useful studio-light directionality and are blended with
		// the surface normal at runtime without another panorama lookup.
		const int diffuseLevel = EnvironmentLevelCount - 1;
		const auto sampleAxis = [&](const Vector3& direction) {
			const float pi = 3.14159265359f;
			const Vector2 uv(
				0.5f + std::atan2(direction.z, direction.x) / (2.0f * pi),
				0.5f - std::asin(std::clamp(direction.y, -1.0f, 1.0f)) / pi);
			const Color sample = sampleTextureBilinearFrom(
				m_filteredEnvironmentLevels[diffuseLevel],
				m_filteredEnvironmentWidths[diffuseLevel],
				m_filteredEnvironmentHeights[diffuseLevel], uv);
			return Vector3(sample.r, sample.g, sample.b);
		};
		m_environmentDiffuseAxes[0] = sampleAxis(Vector3(1.0f, 0.0f, 0.0f));
		m_environmentDiffuseAxes[1] = sampleAxis(Vector3(-1.0f, 0.0f, 0.0f));
		m_environmentDiffuseAxes[2] = sampleAxis(Vector3(0.0f, 1.0f, 0.0f));
		m_environmentDiffuseAxes[3] = sampleAxis(Vector3(0.0f, -1.0f, 0.0f));
		m_environmentDiffuseAxes[4] = sampleAxis(Vector3(0.0f, 0.0f, 1.0f));
		m_environmentDiffuseAxes[5] = sampleAxis(Vector3(0.0f, 0.0f, -1.0f));
	}

	void FragmentShader::setTexture(const std::vector<Color>& texture, int width, int height) {
		m_texture = texture.empty() ? nullptr : &texture;
		m_textureWidth = width;
		m_textureHeight = height;
		m_hasTexture = m_texture != nullptr && width > 0 && height > 0;
	}

	void FragmentShader::setRoughnessTexture(const std::vector<Color>& texture, int width, int height) {
		m_roughnessTexture = texture.empty() ? nullptr : &texture;
		m_roughnessTextureWidth = width;
		m_roughnessTextureHeight = height;
		m_hasRoughnessTexture = m_roughnessTexture != nullptr && width > 0 && height > 0;
	}

	void FragmentShader::setMetallicTexture(const std::vector<Color>& texture, int width, int height) {
		m_metallicTexture = texture.empty() ? nullptr : &texture;
		m_metallicTextureWidth = width;
		m_metallicTextureHeight = height;
		m_hasMetallicTexture = m_metallicTexture != nullptr && width > 0 && height > 0;
	}

	void FragmentShader::setNormalTexture(const std::vector<Color>& texture, int width, int height) {
		m_normalTexture = texture.empty() ? nullptr : &texture;
		m_normalTextureWidth = width;
		m_normalTextureHeight = height;
		m_hasNormalTexture = m_normalTexture != nullptr && width > 0 && height > 0;
	}

	namespace {
	Color sampleTextureBilinearFrom(const std::vector<Color>& texture, int width, int height,
		const Vector2& uv) {
		if (texture.empty() || width <= 0 || height <= 0 ||
			texture.size() < static_cast<size_t>(width) * static_cast<size_t>(height)) {
			return Color::white();
		}
		const Vector2 safeUv(std::isfinite(uv.x) ? uv.x : 0.0f,
			std::isfinite(uv.y) ? uv.y : 0.0f);
		float u = std::fmod(safeUv.x, 1.0f);
		float v = std::fmod(safeUv.y, 1.0f);
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

	Color sampleTextureNearestFrom(const std::vector<Color>& texture, int width, int height,
		const Vector2& uv) {
		if (texture.empty() || width <= 0 || height <= 0 ||
			texture.size() < static_cast<size_t>(width) * static_cast<size_t>(height)) {
			return Color::white();
		}
		const Vector2 safeUv(std::isfinite(uv.x) ? uv.x : 0.0f,
			std::isfinite(uv.y) ? uv.y : 0.0f);
		float u = std::fmod(safeUv.x, 1.0f);
		float v = std::fmod(safeUv.y, 1.0f);
		if (u < 0.0f) u += 1.0f;
		if (v < 0.0f) v += 1.0f;
		const int x = std::clamp(static_cast<int>(u * width), 0, width - 1);
		const int y = std::clamp(static_cast<int>(v * height), 0, height - 1);
		return texture[y * width + x];
	}

	float sampleScalarMap(const std::vector<Color>& texture, int width, int height,
	                     const Vector2& uv, bool enabled) {
		if (!enabled || texture.empty() || width <= 0 || height <= 0 ||
			texture.size() < static_cast<size_t>(width) * static_cast<size_t>(height)) return 1.0f;
		const Vector2 safeUv(std::isfinite(uv.x) ? uv.x : 0.0f,
			std::isfinite(uv.y) ? uv.y : 0.0f);
		float u = std::fmod(safeUv.x, 1.0f);
		float v = std::fmod(safeUv.y, 1.0f);
		if (u < 0.0f) u += 1.0f;
		if (v < 0.0f) v += 1.0f;
		const int x = std::clamp(static_cast<int>(u * width), 0, width - 1);
		const int y = std::clamp(static_cast<int>(v * height), 0, height - 1);
		const Color& sample = texture[y * width + x];
		return (sample.r + sample.g + sample.b) / 3.0f;
	}

	float sampleScalarMapBilinear(const std::vector<Color>& texture, int width, int height,
	                              const Vector2& uv, bool enabled) {
		if (!enabled || texture.empty() || width <= 0 || height <= 0) return 1.0f;
		const Color sample = sampleTextureBilinearFrom(texture, width, height, uv);
		return (sample.r + sample.g + sample.b) / 3.0f;
	}
}

	Color FragmentShader::sampleTexture(const Vector2& uv) {
		return sampleTextureBilinear(uv);
	}

	Color FragmentShader::sampleTextureBilinear(const Vector2& uv) {
		if (!m_hasTexture || !m_texture || m_textureWidth <= 0 || m_textureHeight <= 0 ||
			m_texture->size() < static_cast<size_t>(m_textureWidth) *
			static_cast<size_t>(m_textureHeight)) {
			return Color::white();
		}

		const Vector2 safeUv(std::isfinite(uv.x) ? uv.x : 0.0f,
			std::isfinite(uv.y) ? uv.y : 0.0f);
		float u = std::fmod(safeUv.x, 1.0f);
		float v = std::fmod(safeUv.y, 1.0f);
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

		const Color& c00 = (*m_texture)[wrappedY0 * m_textureWidth + wrappedX0];
		const Color& c10 = (*m_texture)[wrappedY0 * m_textureWidth + wrappedX1];
		const Color& c01 = (*m_texture)[wrappedY1 * m_textureWidth + wrappedX0];
		const Color& c11 = (*m_texture)[wrappedY1 * m_textureWidth + wrappedX1];
		const Color top = Color::lerp(c00, c10, fx);
		const Color bottom = Color::lerp(c01, c11, fx);
		return Color::lerp(top, bottom, fy);
	}

	Color FragmentShader::sampleTextureClamp(const Vector2& uv) {
		if (!m_hasTexture || !m_texture || m_textureWidth <= 0 || m_textureHeight <= 0 ||
			m_texture->size() < static_cast<size_t>(m_textureWidth) *
			static_cast<size_t>(m_textureHeight)) return Color::white();

		float u = clamp(std::isfinite(uv.x) ? uv.x : 0.0f, 0.0f, 1.0f);
		float v = clamp(std::isfinite(uv.y) ? uv.y : 0.0f, 0.0f, 1.0f);

		int x = static_cast<int>(u * (m_textureWidth - 1));
		int y = static_cast<int>(v * (m_textureHeight - 1));

		return (*m_texture)[y * m_textureWidth + x];
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
			vertexOut.tangentSign,
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

	// Metallic-roughness PBR using GGX distribution, Smith visibility and
	// Schlick Fresnel. The public entry point keeps its historical name so
	// existing built-in shader selection remains compatible.
	Color FragmentShader::shadeBlinnPhong(const Fragment& fragment) {
		Vector3 viewDir = (m_viewPosition - fragment.worldPosition).normalized();
		Vector3 shadingNormal = fragment.normal.normalized();
		if (m_hasNormalTexture) {
			const Color normalSample = m_reducedQuality
				? sampleTextureNearestFrom(*m_normalTexture, m_normalTextureWidth,
					m_normalTextureHeight, fragment.texCoord)
				: sampleTextureBilinearFrom(*m_normalTexture, m_normalTextureWidth,
					m_normalTextureHeight, fragment.texCoord);
			// Perspective-correctly interpolate the authored tangent, then
			// Gram-Schmidt it against the interpolated normal. The handedness
			// imported from FBX preserves mirrored UV islands.
			Vector3 tangent = fragment.tangent -
				shadingNormal * shadingNormal.dot(fragment.tangent);
			if (tangent.lengthSquared() <= 1e-8f) {
				const Vector3 helper = std::fabs(shadingNormal.y) < 0.999f
					? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
				tangent = helper.cross(shadingNormal);
			}
			tangent = tangent.normalized();
			const float tangentSign = fragment.tangentSign < 0.0f ? -1.0f : 1.0f;
			const Vector3 bitangent = shadingNormal.cross(tangent).normalized() * tangentSign;
			const float normalStrength = std::max(0.0f, m_material.normalStrength);
			// The editor's repository sample uses Poly Haven's DirectX normal
			// convention, so invert the green channel into our OpenGL-style basis.
			const Vector3 tangentNormal((normalSample.r * 2.0f - 1.0f) * normalStrength,
				(1.0f - normalSample.g * 2.0f) * normalStrength,
				normalSample.b * 2.0f - 1.0f);
			shadingNormal = (tangent * tangentNormal.x + bitangent * tangentNormal.y +
				shadingNormal * tangentNormal.z).normalized();
		}
		Color texColor = m_hasTexture ? sampleTexture(fragment.texCoord) : fragment.color;
		const Color linearTexColor = m_hasTexture ? srgbToLinear(texColor) : texColor;
		const Vector3 baseColor = m_material.diffuse *
			Vector3(linearTexColor.r, linearTexColor.g, linearTexColor.b);
		static const std::vector<Color> emptyTexture;
		const float metallicMap = m_reducedQuality
			? sampleScalarMap(m_hasMetallicTexture ? *m_metallicTexture : emptyTexture,
				m_metallicTextureWidth, m_metallicTextureHeight, fragment.texCoord, m_hasMetallicTexture)
			: sampleScalarMapBilinear(m_hasMetallicTexture ? *m_metallicTexture : emptyTexture,
				m_metallicTextureWidth, m_metallicTextureHeight, fragment.texCoord, m_hasMetallicTexture);
		const float roughnessMap = m_reducedQuality
			? sampleScalarMap(m_hasRoughnessTexture ? *m_roughnessTexture : emptyTexture,
				m_roughnessTextureWidth, m_roughnessTextureHeight, fragment.texCoord, m_hasRoughnessTexture)
			: sampleScalarMapBilinear(m_hasRoughnessTexture ? *m_roughnessTexture : emptyTexture,
				m_roughnessTextureWidth, m_roughnessTextureHeight, fragment.texCoord, m_hasRoughnessTexture);
		const float metallic = clamp(m_material.metallicFactor * metallicMap, 0.0f, 1.0f);
		const float roughness = clamp(m_material.roughness * roughnessMap, 0.02f, 1.0f);
		const float pi = 3.14159265359f;
		const float alpha = roughness * roughness;
		const float alphaSquared = alpha * alpha;
		const Vector3 dielectricF0 = m_material.specular * 0.08f;
		const Vector3 f0 = dielectricF0 * (1.0f - metallic) + baseColor * metallic;
		const float nDotV = std::max(0.0f, shadingNormal.dot(viewDir));
		// Start with the constant environment fallback. When a panorama is
		// present it is split into diffuse irradiance and roughness-filtered
		// specular terms below.
		Vector3 diffuseEnvironment = m_environmentColor * m_environmentIntensity;
		Vector3 specularEnvironment = diffuseEnvironment;
		if (m_hasEnvironmentTexture) {
			const Vector3 reflection = (shadingNormal * (2.0f * shadingNormal.dot(viewDir)) - viewDir).normalized();
			const float pi = 3.14159265359f;
			const auto environmentUv = [&](const Vector3& direction) {
				return Vector2(
					0.5f + std::atan2(direction.z, direction.x) / (2.0f * pi),
					0.5f - std::asin(clamp(direction.y, -1.0f, 1.0f)) / pi);
			};
			const float environmentMip = roughness *
				static_cast<float>(EnvironmentLevelCount - 1);
			const int environmentLevel0 = std::clamp(
				static_cast<int>(std::floor(environmentMip)), 0, EnvironmentLevelCount - 1);
			const int environmentLevel1 = std::min(EnvironmentLevelCount - 1,
				environmentLevel0 + 1);
			const float environmentLevelBlend = environmentMip - static_cast<float>(environmentLevel0);
			const Vector2 reflectionUv = environmentUv(reflection);
			const Color environmentSample0 = sampleTextureBilinearFrom(
				m_filteredEnvironmentLevels[environmentLevel0],
				m_filteredEnvironmentWidths[environmentLevel0],
				m_filteredEnvironmentHeights[environmentLevel0], reflectionUv);
			const Color environmentSample1 = sampleTextureBilinearFrom(
				m_filteredEnvironmentLevels[environmentLevel1],
				m_filteredEnvironmentWidths[environmentLevel1],
				m_filteredEnvironmentHeights[environmentLevel1], reflectionUv);
			const Color environmentSample = Color::lerp(
				environmentSample0, environmentSample1, environmentLevelBlend);
			specularEnvironment = Vector3(environmentSample.r, environmentSample.g, environmentSample.b) *
				m_environmentIntensity;

			const float wx = std::fabs(shadingNormal.x);
			const float wy = std::fabs(shadingNormal.y);
			const float wz = std::fabs(shadingNormal.z);
			const float weightSum = std::max(1e-6f, wx + wy + wz);
			diffuseEnvironment = (
				m_environmentDiffuseAxes[shadingNormal.x >= 0.0f ? 0 : 1] * wx +
				m_environmentDiffuseAxes[shadingNormal.y >= 0.0f ? 2 : 3] * wy +
				m_environmentDiffuseAxes[shadingNormal.z >= 0.0f ? 4 : 5] * wz) *
				(m_environmentIntensity / weightSum);
		}
		const float environmentFresnelFactor = std::pow(1.0f - nDotV, 5.0f);
		const Vector3 environmentFresnel = f0 +
			(Vector3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness) - f0) *
			environmentFresnelFactor;
		// Dielectric surfaces should show a restrained studio reflection; the
		// environment specular term is primarily the energy source for metals.
		// Without this weighting, a dark environment reflection overwhelms the
		// brown albedo of the non-metal grip.
		const float environmentSpecularWeight = metallic + (1.0f - metallic) * 0.05f;
		Vector3 totalLight = m_ambient * m_material.ambient *
			(baseColor * (1.0f - metallic) + f0 * 0.8f) +
			diffuseEnvironment * baseColor * (1.0f - metallic) +
			specularEnvironment * environmentFresnel * environmentSpecularWeight;

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

			const float nDotL = std::max(0.0f, shadingNormal.dot(lightDir));
			if (nDotL <= 0.0f || nDotV <= 0.0f) continue;
			Vector3 halfDir = (lightDir + viewDir).normalized();
			const float nDotH = std::max(0.0f, shadingNormal.dot(halfDir));
			const float vDotH = std::max(0.0f, viewDir.dot(halfDir));
			const float denominator = nDotH * nDotH * (alphaSquared - 1.0f) + 1.0f;
			const float distribution = alphaSquared / (pi * denominator * denominator);
			const float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
			const float geometryV = nDotV / (nDotV * (1.0f - k) + k);
			const float geometryL = nDotL / (nDotL * (1.0f - k) + k);
			const float geometry = geometryV * geometryL;
			const float fresnelFactor = std::pow(1.0f - vDotH, 5.0f);
			const Vector3 fresnel = f0 + (Vector3(1.0f, 1.0f, 1.0f) - f0) * fresnelFactor;
			const Vector3 specular = fresnel * (distribution * geometry /
				std::max(0.0001f, 4.0f * nDotV * nDotL));
			const Vector3 diffuse = baseColor * ((1.0f - metallic) / pi) *
				(Vector3(1.0f, 1.0f, 1.0f) - fresnel);
			const Vector3 lightColor(light.color.r, light.color.g, light.color.b);
			const Vector3 radiance = lightColor * light.intensity * attenuation;
			totalLight = totalLight + (diffuse + specular) * radiance * nDotL;
		}

		Vector3 result = totalLight + m_material.emission;
		if (m_toneMappingEnabled) {
			const Vector3 exposed = result * m_exposure;
			// ACES fitted curve retains highlight shape better than the previous
			// exponential mapping, especially on brushed metal.
			result = Vector3(
				(exposed.x * (2.51f * exposed.x + 0.03f)) /
					(exposed.x * (2.43f * exposed.x + 0.59f) + 0.14f),
				(exposed.y * (2.51f * exposed.y + 0.03f)) /
					(exposed.y * (2.43f * exposed.y + 0.59f) + 0.14f),
				(exposed.z * (2.51f * exposed.z + 0.03f)) /
					(exposed.z * (2.43f * exposed.z + 0.59f) + 0.14f)
			);
			result = Vector3(
				std::pow(std::max(0.0f, result.x), 1.0f / 2.2f),
				std::pow(std::max(0.0f, result.y), 1.0f / 2.2f),
				std::pow(std::max(0.0f, result.z), 1.0f / 2.2f)
			);
		}
		return Color(saturate(result), texColor.a);
	}
}
