#pragma once

#include "Matrix4x4.hpp"
#include "Vector3.hpp"

namespace ST {
	enum class ProjectionMode {
		Perspective,
		Orthographic
	};

	struct Camera {
		Vector3 m_eye;
		Vector3 m_target;
		Vector3 m_up;
		ProjectionMode m_mode;
		float m_fov;
		float m_aspect;
		float m_near;
		float m_far;

		Camera()
			: m_eye(0, 0, 5)
			, m_target(0, 0, 0)
			, m_up(0, 1, 0)
			, m_mode(ProjectionMode::Perspective)
			, m_fov(3.14159f / 4.0f)
			, m_aspect(16.0f / 9.0f)
			, m_near(0.1f)
			, m_far(100.0f) {
		}

		Camera(const Vector3& eye, const Vector3& target)
			: m_eye(eye)
			, m_target(target)
			, m_up(0, 1, 0)
			, m_mode(ProjectionMode::Perspective)
			, m_fov(3.14159f / 4.0f)
			, m_aspect(16.0f / 9.0f)
			, m_near(0.1f)
			, m_far(100.0f) {}

		Matrix4x4 getViewMatrix() const {
			return Matrix4x4::lookAt(m_eye, m_target, m_up);
		}

		Matrix4x4 getProjectionMatrix() const {
			if (m_mode == ProjectionMode::Perspective) {
				return Matrix4x4::perspective(m_fov, m_aspect, m_near, m_far);
			}
			else {
				float halfHeight = 2.0f;
				float halfWidth = halfHeight * m_aspect;
				return Matrix4x4::orthographic(
					-halfWidth, halfWidth,
					-halfHeight, halfHeight,
					m_near, m_far
				);
			}
		}


	};
}