#include "test_geometry.hpp"
#include "Vertex.hpp"
#include "Triangle.hpp"
#include "Mesh.hpp"

namespace ST {
namespace Test {

void testVertex() {
    std::cout << "Testing Vertex..." << std::endl;

    Vertex v1;
    assert(v1.position == Vector3(0, 0, 0));
    assert(v1.normal == Vector3::forward());  // Z+ 朝前
    assert(v1.texCoord == Vector2::zero());
    assert(v1.color == Color::white());

    Vertex v2(Vector3(1, 2, 3), Vector3::right(), Vector2(0.5f, 0.5f), Color::red());
    assert(v2.position.x == 1.0f && v2.position.y == 2.0f && v2.position.z == 3.0f);
    assert(v2.normal == Vector3::right());
    assert(v2.texCoord.x == 0.5f && v2.texCoord.y == 0.5f);
    assert(v2.color == Color::red());

    Vertex v3(Vector3(0, 0, 0));
    assert(v3.position == Vector3::zero());
    assert(v3.normal == Vector3::forward());
    assert(v3.texCoord == Vector2::zero());
    assert(v3.color == Color::white());

    std::cout << "Vertex: PASSED" << std::endl;
}

void testTriangle() {
    std::cout << "Testing Triangle..." << std::endl;

    Triangle t1;
    assert(t1.v0.position == Vector3::zero());
    assert(t1.v1.position == Vector3::zero());
    assert(t1.v2.position == Vector3::zero());

    Vertex v0(Vector3(0, 0, 0));
    Vertex v1(Vector3(1, 0, 0));
    Vertex v2(Vector3(0, 1, 0));
    Triangle t2(v0, v1, v2);

    assert(t2.v0.position == Vector3(0, 0, 0));
    assert(t2.v1.position == Vector3(1, 0, 0));
    assert(t2.v2.position == Vector3(0, 1, 0));

    std::cout << "Triangle: PASSED" << std::endl;
}

void testMesh() {
    std::cout << "Testing Mesh..." << std::endl;

    Mesh mesh;
    assert(mesh.getTriangleCount() == 0);

    Vertex v0(Vector3(0, 0, 0));
    Vertex v1(Vector3(1, 0, 0));
    Vertex v2(Vector3(0, 1, 0));
    mesh.addVertex(v0);
    mesh.addVertex(v1);
    mesh.addVertex(v2);
    mesh.addTriangle(0, 1, 2);
    assert(mesh.getTriangleCount() == 1);

    assert(mesh.getVertexCount() == 3);

    mesh.clear();
    assert(mesh.getTriangleCount() == 0);

    std::cout << "Mesh: PASSED" << std::endl;
}

void testMeshFactory() {
    std::cout << "Testing Mesh Factory..." << std::endl;

    Mesh tri = Mesh::createTriangle();
    assert(tri.getTriangleCount() == 1);

    Mesh plane = Mesh::createPlane(2.0f, 3.0f);
    assert(plane.getTriangleCount() == 2);

    Mesh cube = Mesh::createCube(2.0f);
    assert(cube.getTriangleCount() == 12);

    std::cout << "Mesh Factory: PASSED" << std::endl;
}

} // namespace Test
} // namespace ST
