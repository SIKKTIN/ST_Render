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

        // Box vertices ordered so each face's winding is CCW when viewed
        // from outside the box (i.e. the cross of (v1-v0) x (v2-v0) points
        // away from the box's centre). See docs/rendering/3d-culling-bugs.md
        // for the historical note: the previous winding produced
        // inward-facing face normals, which silently cancelled out the
        // back-face culling sign at the default camera angle but broke at
        // every other yaw / pitch.
        mesh.addVertex(Vertex(Vector3(-h, -h, -h))); // v0
        mesh.addVertex(Vertex(Vector3( h, -h, -h))); // v1
        mesh.addVertex(Vertex(Vector3( h,  h, -h))); // v2
        mesh.addVertex(Vertex(Vector3(-h,  h, -h))); // v3
        mesh.addVertex(Vertex(Vector3(-h, -h,  h))); // v4
        mesh.addVertex(Vertex(Vector3( h, -h,  h))); // v5
        mesh.addVertex(Vertex(Vector3( h,  h,  h))); // v6
        mesh.addVertex(Vertex(Vector3(-h,  h,  h))); // v7

        // CCW from outside (right-hand: normal points outward):
        //   -Z face (z = -h) -- vertices v0 v3 v2 v1
        //   +Z face (z = +h) -- vertices v4 v5 v6 v7
        //   -X face (x = -h) -- vertices v0 v4 v7 v3
        //   +X face (x = +h) -- vertices v1 v2 v6 v5
        //   -Y face (y = -h) -- vertices v0 v1 v5 v4
        //   +Y face (y = +h) -- vertices v3 v7 v6 v2
        int faces[12][3] = {
            {0, 3, 2}, {0, 2, 1},   // -Z
            {4, 5, 6}, {4, 6, 7},   // +Z
            {0, 4, 7}, {0, 7, 3},   // -X
            {1, 2, 6}, {1, 6, 5},   // +X
            {0, 1, 5}, {0, 5, 4},   // -Y
            {3, 7, 6}, {3, 6, 2}    // +Y
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
