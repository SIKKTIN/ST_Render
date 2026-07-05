#pragma once
#define NOMINMAX
#include <algorithm>
#include <cmath>

namespace ST {

	constexpr float PI = 3.14159265358979323846f;
	constexpr float EPSILON = 1e-6f;

	//Degrees -> Radian
	inline float degreesToRadians(float degrees) {
		return degrees * (PI / 180.0f);
	}

	//Radian -> Degrees
	inline float radiansToDegrees(float radians) {
		return radians * (180.0f / PI);
	}

	//Clamp a value between a minimum and maximum
	inline float clamp(float value, float minValue, float maxValue) {
		return std::max(minValue, std::min(value, maxValue));
	}

	//Linear interpolation between two values
	inline float lerp(float a, float b, float t) {
		return a + t * (b - a);
	}

	//Check if a value is approximately zero
	inline bool isZero(float value) {
		return std::abs(value) < EPSILON;
	}

	//Check if two floats are approximately equal
	inline bool isEqual(float a, float b) {
		return std::abs(a - b) < EPSILON;
	}
}