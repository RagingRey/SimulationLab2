#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

class MeshGenerator {
public:
    // Generate a UV sphere
    static Mesh GenerateSphere(float radius, int sectors, int stacks, 
                                glm::vec3 color = {1.0f, 1.0f, 1.0f}) {
        Mesh mesh;
        
        float sectorStep = 2.0f * glm::pi<float>() / sectors;
        float stackStep = glm::pi<float>() / stacks;
        
        for (int i = 0; i <= stacks; ++i) {
            float stackAngle = glm::pi<float>() / 2.0f - i * stackStep;
            float xy = radius * cosf(stackAngle);
            float z = radius * sinf(stackAngle);
            
            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * sectorStep;
                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);
                
                glm::vec3 pos(x, z, y);
                glm::vec3 normal = glm::normalize(pos);
                mesh.vertices.push_back({pos, color, normal});
            }
        }
        
        for (int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;
            
            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                if (i != 0) {
                    mesh.indices.push_back(k1);
                    mesh.indices.push_back(k2);
                    mesh.indices.push_back(k1 + 1);
                }
                if (i != (stacks - 1)) {
                    mesh.indices.push_back(k1 + 1);
                    mesh.indices.push_back(k2);
                    mesh.indices.push_back(k2 + 1);
                }
            }
        }
        return mesh;
    }

    // Generate a cylinder along Y-axis
    static Mesh GenerateCylinder(float radius, float height, int sectors,
                                  glm::vec3 color = {1.0f, 1.0f, 1.0f}) {
        Mesh mesh;
        float halfHeight = height / 2.0f;
        float sectorStep = 2.0f * glm::pi<float>() / sectors;
        
        // Side vertices
        for (int i = 0; i <= 1; ++i) {
            float y = (i == 0) ? -halfHeight : halfHeight;
            for (int j = 0; j <= sectors; ++j) {
                float angle = j * sectorStep;
                float x = radius * cosf(angle);
                float z = radius * sinf(angle);
                glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));
                mesh.vertices.push_back({{x, y, z}, color, normal});
            }
        }
        
        // Side indices
        for (int j = 0; j < sectors; ++j) {
            int k1 = j, k2 = j + sectors + 1;
            mesh.indices.push_back(k1);
            mesh.indices.push_back(k2);
            mesh.indices.push_back(k1 + 1);
            mesh.indices.push_back(k1 + 1);
            mesh.indices.push_back(k2);
            mesh.indices.push_back(k2 + 1);
        }
        
        // Top and bottom caps
        uint32_t baseIndex = static_cast<uint32_t>(mesh.vertices.size());
        
        // Bottom cap center
        mesh.vertices.push_back({{0, -halfHeight, 0}, color, {0, -1, 0}});
        for (int j = 0; j <= sectors; ++j) {
            float angle = j * sectorStep;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);
            mesh.vertices.push_back({{x, -halfHeight, z}, color, {0, -1, 0}});
        }
        for (int j = 0; j < sectors; ++j) {
            mesh.indices.push_back(baseIndex);
            mesh.indices.push_back(baseIndex + j + 2);
            mesh.indices.push_back(baseIndex + j + 1);
        }
        
        baseIndex = static_cast<uint32_t>(mesh.vertices.size());
        
        // Top cap center
        mesh.vertices.push_back({{0, halfHeight, 0}, color, {0, 1, 0}});
        for (int j = 0; j <= sectors; ++j) {
            float angle = j * sectorStep;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);
            mesh.vertices.push_back({{x, halfHeight, z}, color, {0, 1, 0}});
        }
        for (int j = 0; j < sectors; ++j) {
            mesh.indices.push_back(baseIndex);
            mesh.indices.push_back(baseIndex + j + 1);
            mesh.indices.push_back(baseIndex + j + 2);
        }
        
        return mesh;
    }

    // Generate a plane on XZ axis
    static Mesh GeneratePlane(float width, float depth, int divisionsX, int divisionsZ,
                               glm::vec3 color = {0.5f, 0.5f, 0.5f}) {
        Mesh mesh;
        float halfW = width / 2.0f;
        float halfD = depth / 2.0f;
        float stepX = width / divisionsX;
        float stepZ = depth / divisionsZ;
        
        for (int z = 0; z <= divisionsZ; ++z) {
            for (int x = 0; x <= divisionsX; ++x) {
                float px = -halfW + x * stepX;
                float pz = -halfD + z * stepZ;
                mesh.vertices.push_back({{px, 0.0f, pz}, color, {0, 1, 0}});
            }
        }
        
        for (int z = 0; z < divisionsZ; ++z) {
            for (int x = 0; x < divisionsX; ++x) {
                int topLeft = z * (divisionsX + 1) + x;
                int topRight = topLeft + 1;
                int bottomLeft = topLeft + divisionsX + 1;
                int bottomRight = bottomLeft + 1;
                
                mesh.indices.push_back(topLeft);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(topRight);
                mesh.indices.push_back(topRight);
                mesh.indices.push_back(bottomLeft);
                mesh.indices.push_back(bottomRight);
            }
        }
        return mesh;
    }

    // Generate a capsule (cylinder with hemisphere caps) along Y-axis
    static Mesh GenerateCapsule(float radius, float height, int sectors, int rings,
                                 glm::vec3 color = {1.0f, 1.0f, 1.0f}) {
        Mesh mesh;
        float cylinderHeight = height - 2.0f * radius;
        float halfCylinder = cylinderHeight / 2.0f;
        
        // Top hemisphere
        for (int i = 0; i <= rings; ++i) {
            float stackAngle = glm::pi<float>() / 2.0f - i * (glm::pi<float>() / 2.0f) / rings;
            float xy = radius * cosf(stackAngle);
            float y = radius * sinf(stackAngle) + halfCylinder;
            
            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * 2.0f * glm::pi<float>() / sectors;
                float x = xy * cosf(sectorAngle);
                float z = xy * sinf(sectorAngle);
                glm::vec3 normal = glm::normalize(glm::vec3(x, radius * sinf(stackAngle), z));
                mesh.vertices.push_back({{x, y, z}, color, normal});
            }
        }
        
        // Cylinder body
        for (int i = 0; i <= 1; ++i) {
            float y = (i == 0) ? halfCylinder : -halfCylinder;
            for (int j = 0; j <= sectors; ++j) {
                float angle = j * 2.0f * glm::pi<float>() / sectors;
                float x = radius * cosf(angle);
                float z = radius * sinf(angle);
                mesh.vertices.push_back({{x, y, z}, color, glm::normalize(glm::vec3(x, 0, z))});
            }
        }
        
        // Bottom hemisphere
        for (int i = 0; i <= rings; ++i) {
            float stackAngle = -i * (glm::pi<float>() / 2.0f) / rings;
            float xy = radius * cosf(stackAngle);
            float y = radius * sinf(stackAngle) - halfCylinder;
            
            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * 2.0f * glm::pi<float>() / sectors;
                float x = xy * cosf(sectorAngle);
                float z = xy * sinf(sectorAngle);
                glm::vec3 normal = glm::normalize(glm::vec3(x, radius * sinf(stackAngle), z));
                mesh.vertices.push_back({{x, y, z}, color, normal});
            }
        }
        
        // Generate indices for all parts
        int rowCount = (rings + 1) + 2 + (rings + 1);
        for (int i = 0; i < rowCount - 1; ++i) {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;
            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                mesh.indices.push_back(k1);
                mesh.indices.push_back(k2);
                mesh.indices.push_back(k1 + 1);
                mesh.indices.push_back(k1 + 1);
                mesh.indices.push_back(k2);
                mesh.indices.push_back(k2 + 1);
            }
        }
        
        return mesh;
    }
};