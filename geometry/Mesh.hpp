#pragma once

#include "Vertex.hpp"
#include <vector>

namespace ST {
    class Mesh {
    public:
        Mesh() = default;

        void addVertex(const Vertex& vertex);
        void addIndex(int index);

        void addTriangle(int i0, int i1, int i2);

        const std::vector<Vertex>& getVertices() const { return m_vertices; }
        const std::vector<int>& getIndices() const { return m_indices; }

        int getVertexCount() const { return static_cast<int>(m_vertices.size()); }
        int getTriangleCount() const { return static_cast<int>(m_indices.size()) / 3; }
        int getIndexCount() const { return static_cast<int>(m_indices.size()); }

        void clear();

        static Mesh createCube(float size = 1.0f);
        static Mesh createPlane(float width, float height);
        static Mesh createQuad(float width, float height);
        static Mesh createTriangle();
        static Mesh createCircle(float radius, int segments = 32);

    private:
        std::vector<Vertex> m_vertices;
        std::vector<int> m_indices;
    };
}
