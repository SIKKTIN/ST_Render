#include "renderer/geometry/Mesh.hpp"
#include <cmath>

namespace ST {
    void Mesh::addVertex(const Vertex& vertex) {
        m_vertices.push_back(vertex);
    }

    void Mesh::addIndex(int index) {
        m_indices.push_back(index);
    }

    void Mesh::addTriangle(int i0, int i1, int i2) {
        m_indices.push_back(i0);
        m_indices.push_back(i1);
        m_indices.push_back(i2);
    }

    void Mesh::clear() {
        m_vertices.clear();
        m_indices.clear();
    }

    Mesh Mesh::createCube(float size) {
        Mesh mesh;
        float h = size / 2.0f;

        mesh.addVertex(Vertex(Vector3(-h, -h, -h)));
        mesh.addVertex(Vertex(Vector3( h, -h, -h)));
        mesh.addVertex(Vertex(Vector3( h,  h, -h)));
        mesh.addVertex(Vertex(Vector3(-h,  h, -h)));
        mesh.addVertex(Vertex(Vector3(-h, -h,  h)));
        mesh.addVertex(Vertex(Vector3( h, -h,  h)));
        mesh.addVertex(Vertex(Vector3( h,  h,  h)));
        mesh.addVertex(Vertex(Vector3(-h,  h,  h)));

        int faces[12][3] = {
            {0, 1, 2}, {0, 2, 3},
            {5, 4, 7}, {5, 7, 6},
            {4, 0, 3}, {4, 3, 7},
            {1, 5, 6}, {1, 6, 2},
            {3, 2, 6}, {3, 6, 7},
            {4, 5, 1}, {4, 1, 0}
        };

        for (int i = 0; i < 12; ++i) {
            mesh.addTriangle(faces[i][0], faces[i][1], faces[i][2]);
        }

        return mesh;
    }

    Mesh Mesh::createPlane(float width, float height) {
        Mesh mesh;
        float hw = width / 2.0f;
        float hh = height / 2.0f;

        mesh.addVertex(Vertex(Vector3(-hw, -hh, 0)));
        mesh.addVertex(Vertex(Vector3( hw, -hh, 0)));
        mesh.addVertex(Vertex(Vector3( hw,  hh, 0)));
        mesh.addVertex(Vertex(Vector3(-hw,  hh, 0)));

        mesh.addTriangle(0, 1, 2);
        mesh.addTriangle(0, 2, 3);

        return mesh;
    }

    Mesh Mesh::createQuad(float width, float height) {
        return createPlane(width, height);
    }

    Mesh Mesh::createTriangle() {
        Mesh mesh;

        mesh.addVertex(Vertex(Vector3(-1, -1, 0), Vector3::forward(), Vector2::zero(), Color::red()));
        mesh.addVertex(Vertex(Vector3( 1, -1, 0), Vector3::forward(), Vector2(1, 0), Color::green()));
        mesh.addVertex(Vertex(Vector3( 0,  1, 0), Vector3::forward(), Vector2(0.5f, 1), Color::blue()));

        mesh.addTriangle(0, 1, 2);

        return mesh;
    }

    Mesh Mesh::createCircle(float radius, int segments) {
        Mesh mesh;

        mesh.addVertex(Vertex(Vector3(0, 0, 0)));

        for (int i = 0; i < segments; ++i) {
            float angle = 2.0f * 3.1415926535f * i / segments;
            float x = std::cos(angle) * radius;
            float y = std::sin(angle) * radius;
            mesh.addVertex(Vertex(Vector3(x, y, 0)));
        }

        for (int i = 0; i < segments; ++i) {
            mesh.addTriangle(0, i + 1, ((i + 1) % segments) + 1);
        }

        return mesh;
    }
}
