#pragma once

#include "Vertex.hpp"


namespace ST {
	struct Triangle {
	public:
		Vertex v0, v1, v2;
		Triangle() : v0(), v1(), v2() {}
		Triangle(const Vertex& vertex0, const Vertex& vertex1, const Vertex& vertex2)
			: v0(vertex0), v1(vertex1), v2(vertex2) {
		}
	};
}