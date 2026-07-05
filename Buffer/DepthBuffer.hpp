#pragma once

#include <vector>
#include <float.h>

namespace ST {
	class DepthBuffer {
	public:
		DepthBuffer() : m_width(0), m_height(0) {}

		void initialize(int w, int h) {
			m_width = w;
			m_height = h;
			m_depths.resize(w * h, FLT_MAX);
		}

		void clear(float value = FLT_MAX) {
			std::fill(m_depths.begin(), m_depths.end(), value);
		}

		bool testAndSet(int x, int y, float depth) {
			if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
				return false;
			}
			int index = y * m_width + x;
			if (depth < m_depths[index]) {
				m_depths[index] = depth;
				return true;
			}
			return false;
		}

		float getDepth(int x, int y) const {
			if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
				return FLT_MAX;
			}
			return m_depths[y * m_width + x];
		}

		int getWidth() const {
			return m_width;
		}

		int getHeight() const {
			return m_height;
		}	

	private:
		int m_width, m_height;
		std::vector<float> m_depths;
	};
}