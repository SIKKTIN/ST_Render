#pragma once
#include "Vector4.hpp"
#include <algorithm>  

namespace ST {
	class Matrix4x4 {
	public:
		float m[4][4];
		Matrix4x4() {
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					m[i][j] = (i == j) ? 1.0f : 0.0f;
				}
			}
		}
		Matrix4x4(float mat[4][4]) {
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					m[i][j] = mat[i][j];
				}
			}
		}



		Vector4 operator*(const Vector4& vec) const {
			Vector4 result;
			result.x = m[0][0] * vec.x + m[0][1] * vec.y + m[0][2] * vec.z + m[0][3] * vec.w;
			result.y = m[1][0] * vec.x + m[1][1] * vec.y + m[1][2] * vec.z + m[1][3] * vec.w;
			result.z = m[2][0] * vec.x + m[2][1] * vec.y + m[2][2] * vec.z + m[2][3] * vec.w;
			result.w = m[3][0] * vec.x + m[3][1] * vec.y + m[3][2] * vec.z + m[3][3] * vec.w;
			return result;
		}
		Matrix4x4 operator*(const Matrix4x4& other) const {
			Matrix4x4 result;
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					result.m[i][j] = 0.0f;
					for (int k = 0; k < 4; ++k) {
						result.m[i][j] += m[i][k] * other.m[k][j];
					}
				}
			}
			return result;
		}

		Vector3 transformPoint(const Vector3& point) const {
			Vector4 homogenousPoint(point, 1.0f);
			Vector4 transformed = (*this) * homogenousPoint;
			return transformed.toVector3Perspective();
		}

		Vector3 transformDirection(const Vector3& direction) const {
			Vector4 homogenousDirection(direction, 0.0f);
			Vector4 transformed = (*this) * homogenousDirection;
			return transformed.toVector3();
		}

		Matrix4x4 transpose() const {
			Matrix4x4 result;
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					result.m[i][j] = m[j][i];
				}
			}
			return result;
		}

		Matrix4x4 inverse() const {
			Matrix4x4 result = identity();
			Matrix4x4 temp = *this;

			for (int i = 0; i < 4; i++) {
				int maxRow = i;
				for (int k = i + 1; k < 4; k++) {
					if (std::abs(temp.m[k][i]) > std::abs(temp.m[maxRow][i])) {
						maxRow = k;
					}
				}

				if (maxRow != i) {
					std::swap(temp.m[i], temp.m[maxRow]);
					std::swap(result.m[i], result.m[maxRow]);
				}

				if (std::abs(temp.m[i][i]) < 1e-10) {	
					return identity();
				}

				float pivot = temp.m[i][i];
				for (int j = 0; j < 4; j++) {
					temp.m[i][j] /= pivot;
					result.m[i][j] /= pivot;
				}

				for (int k = 0; k < 4; k++) {
					if (k != i) {
						float factor = temp.m[k][i];
						for (int j = 0; j < 4; j++) {
							temp.m[k][j] -= factor * temp.m[i][j];
							result.m[k][j] -= factor * result.m[i][j];
						}
					}
				}
			}

			return result;
		}


		static Matrix4x4 identity() {
			Matrix4x4 result;
			return result;
		}

		static Matrix4x4 zero() {
			Matrix4x4 mat;
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++)
					mat.m[i][j] = 0.0f;
			return mat;
		}

		static Matrix4x4 translation(const Vector3& translation) {
			Matrix4x4 result;
			result.m[0][3] = translation.x;
			result.m[1][3] = translation.y;
			result.m[2][3] = translation.z;
			return result;
		}

		static Matrix4x4 translation(float x, float y, float z) {
			return translation(Vector3(x, y, z));
		}

		static Matrix4x4 scale(float x, float y, float z) {
			Matrix4x4 result;
			result.m[0][0] = x;
			result.m[1][1] = y;
			result.m[2][2] = z;
			return result;
		}

		static Matrix4x4 scale(float scaleValue) {
			return scale(scaleValue, scaleValue, scaleValue);
		}

		static Matrix4x4 rotationX(float angle) {
			Matrix4x4 result;
			float c = std::cos(angle);
			float s = std::sin(angle);
			result.m[1][1] = c;
			result.m[1][2] = -s;
			result.m[2][1] = s;
			result.m[2][2] = c;
			return result;
		}

		static Matrix4x4 rotationY(float angle) {
			Matrix4x4 result;
			float c = std::cos(angle);
			float s = std::sin(angle);
			result.m[0][0] = c;
			result.m[0][2] = s;
			result.m[2][0] = -s;
			result.m[2][2] = c;
			return result;
		}

		static Matrix4x4 rotationZ(float angle) {
			Matrix4x4 result;
			float c = std::cos(angle);
			float s = std::sin(angle);
			result.m[0][0] = c;
			result.m[0][1] = -s;
			result.m[1][0] = s;
			result.m[1][1] = c;
			return result;
		}

		static Matrix4x4 rotation(float yaw, float pitch, float roll) {
			return rotationY(yaw) * rotationX(pitch) * rotationZ(roll);
		}

		static Matrix4x4 perspective(float fov, float aspect, float near, float far) {
			Matrix4x4 result;
			float tanHalfFOV = std::tan(fov / 2.0f);
			result.m[0][0] = 1.0f / (aspect * tanHalfFOV);
			result.m[1][1] = 1.0f / tanHalfFOV;
			result.m[2][2] = -(far + near) / (far - near);
			result.m[2][3] = -(2.0f * far * near) / (far - near);
			result.m[3][2] = -1.0f;
			result.m[3][3] = 0.0f;
			return result;
		}

		static Matrix4x4 orthographic(float left, float right, float bottom, float top, float near, float far) {
			Matrix4x4 result;
			result.m[0][0] = 2.0f / (right - left);
			result.m[1][1] = 2.0f / (top - bottom);
			result.m[2][2] = -2.0f / (far - near);
			result.m[0][3] = -(right + left) / (right - left);
			result.m[1][3] = -(top + bottom) / (top - bottom);
			result.m[2][3] = -(far + near) / (far - near);
			return result;
		}

		static Matrix4x4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up) {
			Vector3 z = (eye - target).normalized();
			Vector3 x = up.cross(z).normalized();
			Vector3 y = z.cross(x);
			Matrix4x4 mat = identity();
			mat.m[0][0] = x.x;  mat.m[0][1] = x.y;  mat.m[0][2] = x.z;
			mat.m[1][0] = y.x;  mat.m[1][1] = y.y;  mat.m[1][2] = y.z;
			mat.m[2][0] = z.x;  mat.m[2][1] = z.y;  mat.m[2][2] = z.z;
			mat.m[0][3] = -x.dot(eye);
			mat.m[1][3] = -y.dot(eye);
			mat.m[2][3] = -z.dot(eye);
			return mat;
		}

		bool operator==(const Matrix4x4& other) const {
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					if (m[i][j] != other.m[i][j]) {
						return false;
					}
				}
			}
			return true;
		}

		bool operator!=(const Matrix4x4& other) const {
			return !(*this == other);
		}
	
	};

	inline Vector4 operator*(const Vector4& vec, const Matrix4x4& mat) {
		return mat * vec;
	}
}