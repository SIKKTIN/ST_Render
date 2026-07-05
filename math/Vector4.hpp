#pragma once
#include "Vector3.hpp"

namespace ST {
	class Vector4 {
	public:
		float x, y, z, w;
		Vector4() : x(0), y(0), z(0), w(1) {}
		Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
		Vector4(const Vector3& vec3, float w) : x(vec3.x), y(vec3.y), z(vec3.z), w(w) {}

		Vector3 toVector3() const {
			return Vector3(x, y, z);
		}

		Vector3 toVector3Perspective() const {
			if (w != 0) {
				return Vector3(x / w, y / w, z / w);
			}
			return Vector3(x, y, z);
		}

		Vector4 operator+(const Vector4& other) const {
			return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
		}
		Vector4 operator-(const Vector4& other) const {
			return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
		}
		Vector4 operator*(float scalar) const {
			return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
		}
		Vector4 operator/(float scalar) const {
			return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
		}
		Vector4& operator+=(const Vector4& other) {
			x += other.x; y += other.y; z += other.z; w += other.w;
			return *this;
		}
		Vector4& operator-=(const Vector4& other) {
			x -= other.x; y -= other.y; z -= other.z; w -= other.w;
			return *this;
		}
		Vector4& operator*=(float scalar) {
			x *= scalar; y *= scalar; z *= scalar; w *= scalar;
			return *this;
		}
		Vector4& operator/=(float scalar) {
			x /= scalar; y /= scalar; z /= scalar; w /= scalar;
			return *this;
		}
		bool operator==(const Vector4& other) const {
			return x == other.x && y == other.y && z == other.z && w == other.w;
		}
		bool operator!=(const Vector4& other) const {
			return !(*this == other);
		}

		float dot(const Vector4& other) const {
			return x * other.x + y * other.y + z * other.z + w * other.w;
		}

		static Vector4 zero() { return Vector4(0, 0, 0, 0); }
		static Vector4 one() { return Vector4(1, 1, 1, 1); }
	};

	class Color {
	public:
		struct RGBProxy {
			Color* cptr;
			RGBProxy(Color* ptr) : cptr(ptr) {}
			operator Vector3() const {
				return Vector3(cptr->r, cptr->g, cptr->b);
			}
			RGBProxy& operator=(const Vector3& v) {
				cptr->r = v.x; cptr->g = v.y; cptr->b = v.z;
				return *this;
			}
			RGBProxy& operator=(const RGBProxy& p) {
				cptr->r = p.cptr->r; cptr->g = p.cptr->g; cptr->b = p.cptr->b;
				return *this;
			}
		};

		RGBProxy rgb;
		float r, g, b, a;

		Color() : rgb(this), r(0), g(0), b(0), a(1) {}
		Color(float cr, float cg, float cb, float ca = 1.0f)
			: rgb(this), r(cr), g(cg), b(cb), a(ca) {}
		Color(const Vector4& vec)
			: rgb(this), r(vec.x), g(vec.y), b(vec.z), a(vec.w) {}
		Color(const Vector3& rgb3, float alpha = 1.0f)
			: rgb(this), r(rgb3.x), g(rgb3.y), b(rgb3.z), a(alpha) {}
		Color(const Color& other)
			: rgb(this), r(other.r), g(other.g), b(other.b), a(other.a) {}
		Color(Color&& other) noexcept
			: rgb(this), r(other.r), g(other.g), b(other.b), a(other.a) {}

		Color& operator=(const Color& other) {
			r = other.r; g = other.g; b = other.b; a = other.a;
			rgb.cptr = this;
			return *this;
		}

		Color& operator=(Color&& other) noexcept {
			r = other.r; g = other.g; b = other.b; a = other.a;
			rgb.cptr = this;
			return *this;
		}

		operator Vector4() const {
			return Vector4(r, g, b, a);
		}

		static Color red()    { return Color(1, 0, 0); }
		static Color green()  { return Color(0, 1, 0); }
		static Color blue()   { return Color(0, 0, 1); }
		static Color white()  { return Color(1, 1, 1); }
		static Color black()  { return Color(0, 0, 0); }
		static Color yellow() { return Color(1, 1, 0); }
		static Color cyan()   { return Color(0, 1, 1); }
		static Color magenta(){ return Color(1, 0, 1); }

		Color operator+(const Color& other) const {
			return Color(r + other.r, g + other.g, b + other.b, a + other.a);
		}
		Color operator-(const Color& other) const {
			return Color(r - other.r, g - other.g, b - other.b, a - other.a);
		}
		Color operator*(float s) const {
			return Color(r * s, g * s, b * s, a * s);
		}
		Color& operator+=(const Color& other) {
			r += other.r; g += other.g; b += other.b; a += other.a;
			return *this;
		}

		bool operator==(const Color& other) const {
			return r == other.r && g == other.g && b == other.b && a == other.a;
		}
		bool operator!=(const Color& other) const {
			return !(*this == other);
		}

		static Color lerp(const Color& a, const Color& b, float t) {
			return Color(
				a.r + (b.r - a.r) * t,
				a.g + (b.g - a.g) * t,
				a.b + (b.b - a.b) * t,
				a.a + (b.a - a.a) * t
			);
		}
	};

	inline Color operator*(float s, const Color& c) {
		return c * s;
	}

	inline Vector4 operator*(float scalar, const Vector4& vector) {
		return vector * scalar;
	}
}
