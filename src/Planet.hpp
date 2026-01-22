#ifndef PLANET_HPP
#define PLANET_HPP

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <cmath>
#include <map>
#include <algorithm>
#include "FastNoiseLite.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Vertex {
    glm::vec3 position;
    float altitude;
    float u, v;
    glm::vec3 normal; // Ajouté pour l'éclairage
};

class Planet {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    unsigned int VBO, VAO, EBO;

    std::vector<float> heightMap;
    int mapWidth = 512;
    int mapHeight = 256;

    Planet() : VAO(0), VBO(0), EBO(0) {}

    void calculateUV(Vertex& v) {
        glm::vec3 n = glm::normalize(v.position);
        v.u = 0.5f + (atan2(n.z, n.x) / (2.0f * (float)M_PI));
        v.v = 0.5f - (asin(n.y) / (float)M_PI);
    }

    unsigned int getMiddlePoint(unsigned int p1, unsigned int p2, std::vector<Vertex>& vertices, std::map<int64_t, unsigned int>& cache) {
        int64_t key = ((int64_t)std::min(p1, p2) << 32) | std::max(p1, p2);
        if (cache.count(key)) return cache[key];

        glm::vec3 v1 = vertices[p1].position;
        glm::vec3 v2 = vertices[p2].position;
        glm::vec3 middle = glm::normalize((v1 + v2) / 2.0f);

        // Initialisation avec normale par défaut (sera recalculée)
        vertices.push_back({middle, 1.0f, 0.0f, 0.0f, middle});
        unsigned int id = (unsigned int)vertices.size() - 1;
        cache[key] = id;
        return id;
    }

    // --- NOUVELLE FONCTION : CALCUL DES NORMALES ---
    void calculateNormals() {
        for (auto& v : vertices) v.normal = glm::vec3(0.0f);

        for (size_t i = 0; i < indices.size(); i += 3) {
            unsigned int i1 = indices[i];
            unsigned int i2 = indices[i+1];
            unsigned int i3 = indices[i+2];

            glm::vec3 v1 = vertices[i1].position;
            glm::vec3 v2 = vertices[i2].position;
            glm::vec3 v3 = vertices[i3].position;

            glm::vec3 edge1 = v2 - v1;
            glm::vec3 edge2 = v3 - v1;
            glm::vec3 faceNormal = glm::cross(edge1, edge2);

            vertices[i1].normal += faceNormal;
            vertices[i2].normal += faceNormal;
            vertices[i3].normal += faceNormal;
        }

        for (auto& v : vertices) {
            v.normal = glm::normalize(v.normal);
        }
    }

    void generateHeightMap() {
        FastNoiseLite noise;
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        heightMap.resize(mapWidth * mapHeight);
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                heightMap[y * mapWidth + x] = (noise.GetNoise((float)x * 2.0f, (float)y * 2.0f) + 1.0f) / 2.0f;
            }
        }
    }

    void generate(int subdivisions) {
        float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
        vertices.clear();
        indices.clear();
        std::vector<glm::vec3> basePos = {
            {-1,t,0}, {1,t,0}, {-1,-t,0}, {1,-t,0}, {0,-1,t}, {0,1,t}, {0,-1,-t}, {0,1,-t}, {t,0,-1}, {t,0,1}, {-t,0,-1}, {-t,0,1}
        };
        for(auto p : basePos) {
            glm::vec3 n = glm::normalize(p);
            vertices.push_back({n, 1.0f, 0.0f, 0.0f, n});
        }
        std::vector<unsigned int> faces = {
            0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11, 1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
            3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9, 4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
        };
        std::map<int64_t, unsigned int> middlePointCache;
        for (int i = 0; i < subdivisions; i++) {
            std::vector<unsigned int> newFaces;
            for (size_t j = 0; j < faces.size(); j += 3) {
                unsigned int a = faces[j], b = faces[j+1], c = faces[j+2];
                unsigned int ab = getMiddlePoint(a, b, vertices, middlePointCache);
                unsigned int bc = getMiddlePoint(b, c, vertices, middlePointCache);
                unsigned int ca = getMiddlePoint(c, a, vertices, middlePointCache);
                newFaces.insert(newFaces.end(), {a, ab, ca,  b, bc, ab,  c, ca, bc,  ab, bc, ca});
            }
            faces = newFaces;
        }
        indices = faces;
    }
    
    void applyRectangularNoise(float strength) {
        for (auto& v : vertices) {
            calculateUV(v);
            int x = (int)(v.u * (float)(mapWidth - 1));
            int y = (int)(v.v * (float)(mapHeight - 1));
            float noiseValue = heightMap[y * mapWidth + x];
            float displacement = 1.0f + (noiseValue * strength);
            v.position = glm::normalize(v.position) * displacement;
            v.altitude = displacement; 
        }
    }

    void setupBuffers() {
        if(VAO == 0) glGenVertexArrays(1, &VAO);
        if(VBO == 0) glGenBuffers(1, &VBO);
        if(EBO == 0) glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // --- CONFIGURATION DES ATTRIBUTS ---

        // Location 0 : Position (vec3)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        // Location 1 : Altitude (float)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, altitude));

        // Location 2 : UV (vec2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

        // Location 3 : Normales (vec3) -> C'est CA qui débloque la lumière !
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glBindVertexArray(0); 
    }

    void draw() {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    }
};

#endif