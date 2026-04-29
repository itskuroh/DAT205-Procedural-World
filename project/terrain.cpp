#include "terrain.h"
#include <glm/gtx/transform.hpp>

// Noise Generator
float hash(float n) { return glm::fract(sin(n) * 43758.5453123f); }

float noise(const glm::vec3& x) {
    glm::vec3 p = glm::floor(x);
    glm::vec3 f = glm::fract(x);
    f = f * f * (3.0f - 2.0f * f);
    float n = p.x + p.y * 57.0f + 113.0f * p.z;
    return glm::mix(glm::mix(glm::mix(hash(n + 0.0f), hash(n + 1.0f), f.x),
        glm::mix(hash(n + 57.0f), hash(n + 58.0f), f.x), f.y),
        glm::mix(glm::mix(hash(n + 113.0f), hash(n + 114.0f), f.x),
            glm::mix(hash(n + 170.0f), hash(n + 171.0f), f.x), f.y), f.z);
}

void Terrain::init(int width, int height, float scale) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    seedX = static_cast<float>(rand() % 10000);
    seedZ = static_cast<float>(rand() % 10000);

    // 1. Generate Positions
    float islandRadius = (width / 2.0f) * scale;
    // version 1: massive island in centre of map
    //for (int z = 0; z <= height; z++) {
    //    for (int x = 0; x <= width; x++) {
    //        float xPos = (static_cast<float>(x) - width / 2.0f) * scale;
    //        float zPos = (static_cast<float>(z) - height / 2.0f) * scale;

    //        float h = noise(glm::vec3(xPos * 0.05f, 0.0f, zPos * 0.05f)) * 12.0f;
    //        h += noise(glm::vec3(xPos * 0.2f, 0.0f, zPos * 0.2f)) * 3.0f;

    //        float dist = glm::length(glm::vec2(xPos, zPos));
    //        float mask = glm::smoothstep(islandRadius, islandRadius * 0.4f, dist);
    //        float yPos = (h * mask) - 1.5f;

    //        vertices.push_back({ glm::vec3(xPos, yPos, zPos), glm::vec3(0, 1, 0) });
    //    }
    //}

    // version 2: massive mountains
    for (int z = 0; z <= height; z++) {
        for (int x = 0; x <= width; x++) {
            float xPos = (static_cast<float>(x) - width / 2.0f) * scale;
            float zPos = (static_cast<float>(z) - height / 2.0f) * scale;

            float freq = 0.012f;
            float base = noise(glm::vec3((x + seedX) * freq, 0.0f, (z + seedZ) * freq));

            float centered = (base * 2.0f) - 1.0f;

            float h = 0.0f;

            if (centered > 0.0f) {
                h = std::pow(centered, 3.0f) * 150.0f;

                // add jagged detail only to mountains
                float detail = 1.0f - std::abs(noise(glm::vec3(x * 0.1f, 0.0f, z * 0.1f)));
                h += detail * 20.0f * centered;
            }
            else {
                //ocean
                float depth = std::pow(std::abs(centered), 0.7f);
                h = -depth * 180.0f;
            }

            // 3. Final Y Position (No need to subtract 20 anymore, h = 0 is now natural sea level)
            float yPos = h;

            vertices.push_back({ glm::vec3(xPos, yPos, zPos), glm::vec3(0, 1, 0) });
        }
    }

    // 2. Generate Indices & Calculate Normals
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            uint32_t i0 = z * (width + 1) + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = (z + 1) * (width + 1) + x;
            uint32_t i3 = i2 + 1;

            // Create two triangles for the quad
            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);

            // CALCULATE REAL NORMALS
            // We take the cross product of the actual generated geometry
            glm::vec3 v0 = vertices[i0].position;
            glm::vec3 v1 = vertices[i1].position;
            glm::vec3 v2 = vertices[i2].position;

            // This cross product determines the "slope" direction for the light
            glm::vec3 normal = glm::normalize(glm::cross(v2 - v0, v1 - v0));

            // Assign normal to the vertices of this triangle
            vertices[i0].normal = normal;
            vertices[i1].normal = normal;
            vertices[i2].normal = normal;
        }
    }

    numIndices = static_cast<int>(indices.size());

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // Matches layout(location = 0) in your shading.vert
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Matches layout(location = 1) in your shading.vert (normalIn)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(sizeof(glm::vec3)));
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void Terrain::render() {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, nullptr);
}

float Terrain::getHeightAt(float xPos, float zPos, float scale) {
    float w = 1000.0f;
    float h_map = 1000.0f;

    float x = (xPos / scale) + w / 2.0f;
    float z = (zPos / scale) + h_map / 2.0f;

    float freq = 0.012f;
    float base = noise(glm::vec3((x + seedX) * freq, 0.0f, (z + seedZ) * freq));

    float centered = (base * 2.0f) - 1.0f;
    float h = 0.0f;

    if (centered > 0.0f) {
        // Mountains
        h = std::pow(centered, 3.0f) * 150.0f;

        // Add detail
        float detail = 1.0f - std::abs(noise(glm::vec3(x * 0.1f, 0.0f, z * 0.1f)));
        h += detail * 20.0f * centered;
    }
    else {
        // Ocean
        float depth = std::pow(std::abs(centered), 0.7f);
        h = -depth * 180.0f;
    }

    return h;
}