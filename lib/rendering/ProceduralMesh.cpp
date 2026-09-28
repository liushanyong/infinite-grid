#include "ProceduralMesh.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <utility>

namespace rendering
{
namespace
{

void appendTriangle(std::vector<float> &out, glm::vec3 a, glm::vec3 b,
                    glm::vec3 c, const glm::vec3 &outward, glm::vec2 ua,
                    glm::vec2 ub, glm::vec2 uc)
{
    const glm::vec3 crossAB = glm::cross(b - a, c - a);
    if (glm::dot(crossAB, crossAB) < 1e-12f)
        return; // Degenerate sphere pole quads collapse to triangles.
    if (glm::dot(crossAB, outward) < 0.0f)
        std::swap(b, c);

    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    const glm::vec3 *vertices[] = {&a, &b, &c};
    const glm::vec2 *uvs[] = {&ua, &ub, &uc};
    for (size_t i = 0; i < 3; ++i)
    {
        out.push_back(vertices[i]->x);
        out.push_back(vertices[i]->y);
        out.push_back(vertices[i]->z);
        out.push_back(normal.x);
        out.push_back(normal.y);
        out.push_back(normal.z);
        out.push_back(uvs[i]->x);
        out.push_back(uvs[i]->y);
    }
}

glm::vec2 sphericalUv(float v, float u)
{
    return glm::vec2(u, 1.0f - v);
}

std::vector<float> makeCubeMesh()
{
    constexpr float kSize = 0.5f;
    std::vector<float> out;
    out.reserve(36 * kProceduralMeshFloatStride);

    auto appendQuad = [&](const glm::vec3 &a, const glm::vec3 &b,
                          const glm::vec3 &c, const glm::vec3 &d,
                          const glm::vec3 &normal) {
        appendTriangle(out, a, b, c, normal,
                       glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f),
                       glm::vec2(1.0f, 0.0f));
        appendTriangle(out, a, c, d, normal,
                       glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 0.0f),
                       glm::vec2(0.0f, 0.0f));
    };

    appendQuad({-kSize, -kSize, kSize}, {kSize, -kSize, kSize},
               {kSize, kSize, kSize}, {-kSize, kSize, kSize}, {0, 0, 1});
    appendQuad({kSize, -kSize, -kSize}, {-kSize, -kSize, -kSize},
               {-kSize, kSize, -kSize}, {kSize, kSize, -kSize}, {0, 0, -1});
    appendQuad({kSize, -kSize, kSize}, {kSize, -kSize, -kSize},
               {kSize, kSize, -kSize}, {kSize, kSize, kSize}, {1, 0, 0});
    appendQuad({-kSize, -kSize, -kSize}, {-kSize, -kSize, kSize},
               {-kSize, kSize, kSize}, {-kSize, kSize, -kSize}, {-1, 0, 0});
    appendQuad({-kSize, kSize, kSize}, {kSize, kSize, kSize},
               {kSize, kSize, -kSize}, {-kSize, kSize, -kSize}, {0, 1, 0});
    appendQuad({-kSize, -kSize, -kSize}, {kSize, -kSize, -kSize},
               {kSize, -kSize, kSize}, {-kSize, -kSize, kSize}, {0, -1, 0});
    return out;
}

std::vector<float> makeSphereMesh()
{
    std::vector<float> out;
    constexpr int kStacks = 12;
    constexpr int kSlices = 16;
    constexpr float kRadius = 0.5f;
    out.reserve(static_cast<size_t>(kStacks * kSlices * 2) *
                kProceduralMeshFloatStride);
    const float kPi = glm::pi<float>();

    auto point = [kRadius, kStacks, kSlices, kPi](int stack, int slice) {
        const float v = kPi * static_cast<float>(stack) /
                        static_cast<float>(kStacks);
        const float u = 2.0f * kPi * static_cast<float>(slice) /
                        static_cast<float>(kSlices);
        return glm::vec3(kRadius * std::sin(v) * std::cos(u),
                         kRadius * std::cos(v),
                         kRadius * std::sin(v) * std::sin(u));
    };

    for (int stack = 0; stack < kStacks; ++stack)
    {
        for (int slice = 0; slice < kSlices; ++slice)
        {
            const glm::vec3 a = point(stack, slice);
            const glm::vec3 b = point(stack + 1, slice);
            const glm::vec3 c = point(stack + 1, slice + 1);
            const glm::vec3 d = point(stack, slice + 1);
            const float v0 = static_cast<float>(stack) / kStacks;
            const float v1 = static_cast<float>(stack + 1) / kStacks;
            const float u0 = static_cast<float>(slice) / kSlices;
            const float u1 = static_cast<float>(slice + 1) / kSlices;
            appendTriangle(out, a, b, c, a, sphericalUv(v0, u0),
                           sphericalUv(v1, u0), sphericalUv(v1, u1));
            appendTriangle(out, a, c, d, a, sphericalUv(v0, u0),
                           sphericalUv(v1, u1), sphericalUv(v0, u1));
        }
    }
    return out;
}

std::vector<float> makeConeMesh()
{
    std::vector<float> out;
    constexpr int kSegments = 16;
    constexpr float kRadius = 0.5f;
    constexpr float kHalfHeight = 0.5f;
    out.reserve(static_cast<size_t>(kSegments * 2) *
                kProceduralMeshFloatStride);

    const glm::vec3 apex(0.0f, kHalfHeight, 0.0f);
    const float kPi = glm::pi<float>();
    auto rim = [kRadius, kHalfHeight, kSegments, kPi](int segment) {
        const float phi = 2.0f * kPi * static_cast<float>(segment) /
                          static_cast<float>(kSegments);
        return glm::vec3(kRadius * std::cos(phi), -kHalfHeight,
                         kRadius * std::sin(phi));
    };

    for (int segment = 0; segment < kSegments; ++segment)
    {
        const glm::vec3 p0 = rim(segment);
        const glm::vec3 p1 = rim(segment + 1);
        const glm::vec3 mid = 0.5f * (p0 + p1);
        const float u0 = static_cast<float>(segment) / kSegments;
        const float u1 = static_cast<float>(segment + 1) / kSegments;
        appendTriangle(out, apex, p0, p1, glm::vec3(mid.x, 0.0f, mid.z),
                       glm::vec2(0.5f * (u0 + u1), 1.0f),
                       glm::vec2(u0, 0.0f), glm::vec2(u1, 0.0f));
        appendTriangle(out, glm::vec3(0.0f, -kHalfHeight, 0.0f), p0, p1,
                       glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.5f, 0.5f),
                       glm::vec2(u0, 0.0f), glm::vec2(u1, 1.0f));
    }
    return out;
}

std::vector<float> makeTorusMesh()
{
    std::vector<float> out;
    constexpr int kMajor = 16;
    constexpr int kMinor = 8;
    constexpr float kMajorRadius = 0.325f;
    constexpr float kMinorRadius = 0.175f;
    out.reserve(static_cast<size_t>(kMajor * kMinor * 2) *
                kProceduralMeshFloatStride);
    const float kPi = glm::pi<float>();

    auto point = [kMajorRadius, kMinorRadius, kMajor, kMinor,
                  kPi](int major, int minor, glm::vec3 *normalOut) {
        const float u = 2.0f * kPi * static_cast<float>(major) /
                        static_cast<float>(kMajor);
        const float v = 2.0f * kPi * static_cast<float>(minor) /
                        static_cast<float>(kMinor);
        const float cu = std::cos(u);
        const float su = std::sin(u);
        const float cv = std::cos(v);
        const float sv = std::sin(v);
        if (normalOut)
            *normalOut = glm::vec3(cv * cu, sv, cv * su);
        return glm::vec3((kMajorRadius + kMinorRadius * cv) * cu,
                         kMinorRadius * sv,
                         (kMajorRadius + kMinorRadius * cv) * su);
    };

    for (int major = 0; major < kMajor; ++major)
    {
        for (int minor = 0; minor < kMinor; ++minor)
        {
            glm::vec3 normalA;
            const glm::vec3 a = point(major, minor, &normalA);
            const glm::vec3 b = point(major + 1, minor, nullptr);
            const glm::vec3 c = point(major + 1, minor + 1, nullptr);
            const glm::vec3 d = point(major, minor + 1, nullptr);
            const float u0 = static_cast<float>(major) / kMajor;
            const float u1 = static_cast<float>(major + 1) / kMajor;
            const float v0 = static_cast<float>(minor) / kMinor;
            const float v1 = static_cast<float>(minor + 1) / kMinor;
            const glm::vec2 uvA(u0, v0);
            const glm::vec2 uvB(u1, v0);
            const glm::vec2 uvC(u1, v1);
            const glm::vec2 uvD(u0, v1);
            appendTriangle(out, a, b, c, normalA, uvA, uvB, uvC);
            appendTriangle(out, a, c, d, normalA, uvA, uvC, uvD);
        }
    }
    return out;
}

} // namespace

const std::vector<float> &proceduralMeshVertices(MeshType mesh)
{
    static const std::array<std::vector<float>, 4> meshes = {
        makeCubeMesh(), makeSphereMesh(), makeConeMesh(), makeTorusMesh()};
    return meshes[static_cast<size_t>(mesh)];
}

const std::vector<float> &proceduralMeshFeatureEdges(MeshType mesh)
{
    static const std::vector<float> cubeEdges = [] {
        struct Edge { glm::vec3 a; glm::vec3 b; glm::vec3 normal; };
        const std::array<Edge, 12> edges{{
            {{-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, {0.0f,0.0f, 1.0f}},
            {{ 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {0.0f,0.0f, 1.0f}},
            {{ 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {0.0f,0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}, {0.0f,0.0f, 1.0f}},
            {{-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, {0.0f,0.0f,-1.0f}},
            {{ 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, {0.0f,0.0f,-1.0f}},
            {{ 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, {0.0f,0.0f,-1.0f}},
            {{-0.5f, 0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {0.0f,0.0f,-1.0f}},
            {{-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-1.0f,0.0f,0.0f}},
            {{ 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, { 1.0f,0.0f,0.0f}},
            {{ 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}, { 1.0f,0.0f,0.0f}},
            {{-0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f, 0.5f}, {-1.0f,0.0f,0.0f}}}};
        std::vector<float> result;
        result.reserve(edges.size() * 16);
        for (const Edge &edge : edges)
        {
            for (const glm::vec3 &position : {edge.a, edge.b})
            {
                result.insert(result.end(), {position.x, position.y, position.z,
                    edge.normal.x, edge.normal.y, edge.normal.z, 0.0f, 0.0f});
            }
        }
        return result;
    }();

    static const std::vector<float> sphereEdges = [] {
        constexpr int kStacks = 12;
        constexpr int kSlices = 16;
        constexpr float kRadius = 0.5f;
        auto point = [](int stack, int slice) {
            const float v = glm::pi<float>() * float(stack) / float(kStacks);
            const float u = glm::two_pi<float>() * float(slice) / float(kSlices);
            return glm::vec3(kRadius * std::sin(v) * std::cos(u),
                             kRadius * std::cos(v),
                             kRadius * std::sin(v) * std::sin(u));
        };
        std::vector<float> result;
        auto append = [&](const glm::vec3 &a, const glm::vec3 &b) {
            for (const glm::vec3 &position : {a, b})
                result.insert(result.end(), {position.x, position.y, position.z,
                    position.x, position.y, position.z, 0.0f, 0.0f});
        };
        for (int slice = 0; slice < kSlices; slice += 2)
            for (int stack = 0; stack < kStacks; ++stack)
                append(point(stack, slice), point(stack + 1, slice));
        for (int stack = 3; stack < kStacks; stack += 3)
            for (int slice = 0; slice < kSlices; ++slice)
                append(point(stack, slice), point(stack, slice + 1));
        return result;
    }();

    static const std::vector<float> coneEdges = [] {
        constexpr int kSegments = 16;
        constexpr float kRadius = 0.5f;
        constexpr float kHalfHeight = 0.5f;
        const glm::vec3 apex(0.0f, kHalfHeight, 0.0f);
        auto rim = [&](int segment) {
            const float phi = glm::two_pi<float>() * float(segment) / float(kSegments);
            return glm::vec3(kRadius * std::cos(phi), -kHalfHeight,
                             kRadius * std::sin(phi));
        };
        std::vector<float> result;
        auto append = [&](const glm::vec3 &a, const glm::vec3 &normalA,
                          const glm::vec3 &b, const glm::vec3 &normalB) {
            for (const auto &vertex : {std::pair{a, normalA}, std::pair{b, normalB}})
                result.insert(result.end(), {vertex.first.x, vertex.first.y,
                    vertex.first.z, vertex.second.x, vertex.second.y,
                    vertex.second.z, 0.0f, 0.0f});
        };
        for (int segment = 0; segment < kSegments; ++segment)
        {
            const glm::vec3 a = rim(segment);
            const glm::vec3 b = rim(segment + 1);
            append(a, glm::vec3(a.x, 0.0f, a.z), b, glm::vec3(b.x, 0.0f, b.z));
        }
        for (int segment = 0; segment < kSegments; segment += 4)
        {
            const glm::vec3 p = rim(segment);
            const glm::vec3 normal = glm::normalize(apex - p);
            append(apex, normal, p, glm::vec3(p.x, 0.0f, p.z));
        }
        return result;
    }();

    static const std::vector<float> torusEdges = [] {
        constexpr int kMajor = 16;
        constexpr int kMinor = 8;
        constexpr float kMajorRadius = 0.325f;
        constexpr float kMinorRadius = 0.175f;
        auto point = [](int major, int minor) {
            const float u = glm::two_pi<float>() * float(major) / float(kMajor);
            const float v = glm::two_pi<float>() * float(minor) / float(kMinor);
            const float cu = std::cos(u), su = std::sin(u);
            const float cv = std::cos(v), sv = std::sin(v);
            return glm::vec3((kMajorRadius + kMinorRadius * cv) * cu,
                             kMinorRadius * sv,
                             (kMajorRadius + kMinorRadius * cv) * su);
        };
        auto normal = [](int major, int minor) {
            const float u = glm::two_pi<float>() * float(major) / float(kMajor);
            const float v = glm::two_pi<float>() * float(minor) / float(kMinor);
            return glm::vec3(std::cos(v) * std::cos(u), std::sin(v),
                             std::cos(v) * std::sin(u));
        };
        std::vector<float> result;
        auto append = [&](const glm::vec3 &a, const glm::vec3 &na,
                          const glm::vec3 &b, const glm::vec3 &nb) {
            for (const auto &vertex : {std::pair{a, na}, std::pair{b, nb}})
                result.insert(result.end(), {vertex.first.x, vertex.first.y,
                    vertex.first.z, vertex.second.x, vertex.second.y,
                    vertex.second.z, 0.0f, 0.0f});
        };
        for (const int minor : {0, kMinor / 2})
            for (int major = 0; major < kMajor; ++major)
                append(point(major, minor), normal(major, minor),
                       point(major + 1, minor), normal(major + 1, minor));
        for (const int major : {0, kMajor / 4, kMajor / 2, kMajor * 3 / 4})
            for (int minor = 0; minor < kMinor; ++minor)
                append(point(major, minor), normal(major, minor),
                       point(major, minor + 1), normal(major, minor + 1));
        return result;
    }();

    switch (mesh)
    {
    case MeshType::Sphere: return sphereEdges;
    case MeshType::Cone: return coneEdges;
    case MeshType::Torus: return torusEdges;
    case MeshType::Cube: break;
    }
    return cubeEdges;
}

} // namespace rendering
