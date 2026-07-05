#pragma once

#include "Vector4.hpp"
#include <vector>

namespace ST {
	class FrameBuffer {
	public:
		FrameBuffer() : m_width(0), m_height(0) {}
		
		void initialize(int width, int height) {
			m_width = width;
			m_height = height;
			m_pixels.resize(width * height);
			clear(Color::black());
		}

		void clear(const Color& color) {
			std::fill(m_pixels.begin(), m_pixels.end(), color);
		}

		void setPixel(int x, int y, const Color& color) {
			if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
				m_pixels[y * m_width + x] = color;
			}
		}
		
		Color getPixel(int x, int y) const {
			if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
				return m_pixels[y * m_width + x];
			}
			return Color::black();
		}
		
		int getWidth() const {
			return m_width;
		}

		int getHeight() const {
			return m_height;
		}

		const std::vector<Color>& getPixels() const {
			return m_pixels;
		}
		std::vector<Color>& getPixels() {
			return m_pixels;
		}
	private:
		int m_width;
		int m_height;
		std::vector<Color> m_pixels;
	};
}