#pragma once  
#include "MathUtils.hpp"  

namespace ST {  
class Vector2 {  
public:  
	float x, y;  

	Vector2() : x(0), y(0) {}  
	Vector2(float x, float y) : x(x), y(y) {}  
	Vector2(float value) : x(value), y(value) {}  

	Vector2 operator+(const Vector2& other) const {  
		return Vector2(x + other.x, y + other.y);  
	}  

	Vector2 operator-(const Vector2& other) const {  
		return Vector2(x - other.x, y - other.y);  
	}  

	Vector2 operator*(float scalar) const {  
		return Vector2(x * scalar, y * scalar);  
	}  

	Vector2 operator/(float scalar) const {  
		return Vector2(x / scalar, y / scalar);  
	}  

	Vector2& operator+=(const Vector2& other) {  
		x += other.x;  
		y += other.y;  
		return *this;  
	}  

	Vector2& operator-=(const Vector2& other) {  
		x -= other.x;  
		y -= other.y;  
		return *this;  
	}  

	Vector2& operator*=(float scalar) {  
		x *= scalar;  
		y *= scalar;  
		return *this;  
	}  

	Vector2& operator/=(float scalar) {  
		x /= scalar;  
		y /= scalar;  
		return *this;  
	}  

	bool operator==(const Vector2& other) const {  
		return isEqual(x, other.x) && isEqual(y, other.y);  
	}  

	bool operator!=(const Vector2& other) const {  
		return !(*this == other);  
	}  

	float dot(const Vector2& other) const {
		return x * other.x + y * other.y;
	}

	float length() const {
		return std::sqrt(x * x + y * y);
	}

	float lengthSquared() const {
		return x * x + y * y;
	}

	Vector2 normalized() const {
		float len = length();
		if (!isZero(len)) {
			return Vector2(x / len, y / len);
		}
		return Vector2(0, 0);
	}

	void normalize() {
		float len = length();
		if (!isZero(len)) {
			x /= len;
			y /= len;
		}
	}

	static Vector2 lerp(const Vector2& a, const Vector2& b, float t) {
		return a + (b - a) * t;
	}

		static Vector2 zero() {
			return Vector2(0, 0);
		}

		static Vector2 one() {
			return Vector2(1, 1);
		}

		static Vector2 right() {
			return Vector2(1, 0);
		}

		static Vector2 up() {
			return Vector2(0, 1);
		}

};

inline Vector2 operator*(float scalar, const Vector2& vector) {
	return vector * scalar;
}

}