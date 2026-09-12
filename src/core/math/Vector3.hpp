#pragma once
#include<cmath>

namespace ST {
	class Vector3 {
	public:
		float x, y, z;

		Vector3() : x(0), y(0), z(0) {}
		Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

		Vector3 operator+(const Vector3& other) const {
			return Vector3(x + other.x, y + other.y, z + other.z);
		}

		Vector3 operator-(const Vector3& other) const {
			return Vector3(x - other.x, y - other.y, z - other.z);
		}

		Vector3 operator-() const {
			return Vector3(-x, -y, -z);
		}

		Vector3 operator*(float scalar) const {
			return Vector3(x * scalar, y * scalar, z * scalar);
		}

		Vector3 operator*(const Vector3& other) const {
			return Vector3(x * other.x, y * other.y, z * other.z);
		}

		Vector3 operator/(float scalar) const {
			return Vector3(x / scalar, y / scalar, z / scalar);
		}

		Vector3& operator+=(const Vector3& other) {
			x += other.x;
			y += other.y;
			z += other.z;
			return *this;
		}

		Vector3& operator-=(const Vector3& other) {
			x -= other.x;
			y -= other.y;
			z -= other.z;
			return *this;
		}

		Vector3& operator*=(float scalar) {
			x *= scalar;
			y *= scalar;
			z *= scalar;
			return *this;
		}

		Vector3& operator/=(float scalar) {
			x /= scalar;
			y /= scalar;
			z /= scalar;
			return *this;
		}

		bool operator==(const Vector3& other) const {
			return x == other.x && y == other.y && z == other.z;
		}

		bool operator!=(const Vector3& other) const {
			return !(*this == other);
		}

		float dot(const Vector3& other) const {
			return x * other.x + y * other.y + z * other.z;
		}


		Vector3 cross(const Vector3& other) const {
			return Vector3(
				y * other.z - z * other.y,
				z * other.x - x * other.z,
				x * other.y - y * other.x
			);
		}

		float length() const {
			return std::sqrt(x * x + y * y + z * z);
		}

		float lengthSquared() const {
			return x * x + y * y + z * z;
		}

		Vector3 normalized() const {
			float len = length();
			if (len > 0) {
				return *this / len;
			}
			return Vector3(0, 0, 0);
		}

		void normalize() {
			float len = length();
			if (len > 0) {
				x /= len;
				y /= len;
				z /= len;
			}
		}

		static float dot(const Vector3& a, const Vector3& b) {
			return a.dot(b);
		}

		static Vector3 cross(const Vector3& a, const Vector3& b) {
			return a.cross(b);
		}

		static Vector3 zero() {
			return Vector3(0, 0, 0);
		}

		static Vector3 up() {
			return Vector3(0, 1, 0);
		}

		static Vector3 right() {
			return Vector3(1, 0, 0);
		}

		static Vector3 forward() {
			return Vector3(0, 0, 1);
		}
	};

	inline Vector3 operator* (float scalar, const Vector3& vector) {
		return vector * scalar;
	}
}