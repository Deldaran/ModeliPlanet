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
    float radius;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    unsigned int VBO, VAO, EBO;

    std::vector<float> heightMap;
    int mapWidth = 512;
    int mapHeight = 256;
    float relativeStrength = 0.0f;

    Planet(float radius) : radius(radius), VAO(0), VBO(0), EBO(0) {}

    void calculateUV(Vertex& v) {
        glm::vec3 n = glm::normalize(v.position);
        v.u = 0.5f + (atan2(n.z, n.x) / (2.0f * (float)M_PI));
        v.v = 0.5f - (asin(n.y) / (float)M_PI);
    }
    
    float calculateNoiseAt(glm::vec3 dir) {
        // 1. Si la heightMap est vide, on renvoie une valeur neutre
        if (heightMap.empty()) return 0.5f;

        // 2. On transforme la direction 3D en coordonnées UV (0 à 1)
        // C'est exactement la même projection que pour ton mesh visuel
        glm::vec3 n = glm::normalize(dir);
        float u = 0.5f + (atan2(n.z, n.x) / (2.0f * (float)M_PI));
        float v = 0.5f - (asin(n.y) / (float)M_PI);

        // 3. On convertit ces UV en indices de pixels dans ta heightMap
        int x = (int)(u * (mapWidth - 1));
        int y = (int)(v * (mapHeight - 1));
        
        // Sécurité pour ne pas sortir du tableau
        x = std::clamp(x, 0, mapWidth - 1);
        y = std::clamp(y, 0, mapHeight - 1);

        // 4. On renvoie la valeur stockée dans la carte de hauteur
        return heightMap[y * mapWidth + x];
    }

    // Échantillonnage bilinéaire de la heightMap à partir d'UV (0..1)
    float sampleHeightMap(float u, float v) {
        if (heightMap.empty()) return 0.5f;

        u = glm::clamp(u, 0.0f, 1.0f);
        v = glm::clamp(v, 0.0f, 1.0f);

        float mapX = u * (float)(mapWidth - 1);
        float mapY = v * (float)(mapHeight - 1);

        int x0 = (int)std::floor(mapX);
        int y0 = (int)std::floor(mapY);
        int x1 = (x0 + 1) % mapWidth;
        int y1 = (y0 + 1) % mapHeight;

        float sx = mapX - (float)x0;
        float sy = mapY - (float)y0;

        float v00 = heightMap[y0 * mapWidth + x0];
        float v10 = heightMap[y0 * mapWidth + x1];
        float v01 = heightMap[y1 * mapWidth + x0];
        float v11 = heightMap[y1 * mapWidth + x1];

        float top = v00 + sx * (v10 - v00);
        float bottom = v01 + sx * (v11 - v01);

        return top + sy * (bottom - top);
    }

    float getSurfaceHeight(glm::vec3 direction) {
        // Convertit la direction en UV, échantillonne la heightMap et renvoie le rayon réel
        glm::vec3 n = glm::normalize(direction);
        float u = 0.5f + (atan2(n.z, n.x) / (2.0f * (float)M_PI));
        float v = 0.5f - (asin(n.y) / (float)M_PI);

        float noiseValue = sampleHeightMap(u, v);

        float displacement;
        if (noiseValue < 0.5f) {
            displacement = 1.0f;
        } else {
            displacement = 1.0f + (noiseValue - 0.5f) * this->relativeStrength;
        }

        return radius * displacement;
    }

    unsigned int getMiddlePoint(unsigned int p1, unsigned int p2, std::vector<Vertex>& vertices, std::map<int64_t, unsigned int>& cache) {
        int64_t key = ((int64_t)std::min(p1, p2) << 32) | std::max(p1, p2);
        if (cache.count(key)) return cache[key];

        glm::vec3 v1 = vertices[p1].position;
        glm::vec3 v2 = vertices[p2].position;
        glm::vec3 middle = glm::normalize((v1 + v2) / 2.0f) * radius;

        // Initialisation avec normale par défaut (sera recalculée)
        vertices.push_back({middle, 1.0f, 0.0f, 0.0f, glm::normalize(middle)});
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
        noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        noise.SetFractalOctaves(6);
        noise.SetFrequency(0.8f); // Ajuste selon la taille voulue des continents

        heightMap.resize(mapWidth * mapHeight);

        for (int y = 0; y < mapHeight; y++) {
            // v va de 0 (pôle nord) à 1 (pôle sud)
            float v = (float)y / (float)(mapHeight - 1);
            // On calcule la latitude (phi) de 0 à PI
            float phi = v * (float)M_PI;

            for (int x = 0; x < mapWidth; x++) {
                // u va de 0 à 1 (tour complet)
                float u = (float)x / (float)(mapWidth - 1);
                // On calcule la longitude (theta) de 0 à 2*PI
                float theta = u * 2.0f * (float)M_PI;

                // --- PROJECTION SPHÉRIQUE PARFAITE (Pas de déformation) ---
                // On transforme (u, v) en coordonnées (x, y, z) sur une sphère
                float nx = sin(phi) * cos(theta);
                float ny = cos(phi);           // Axe vertical (pôles)
                float nz = sin(phi) * sin(theta);

                // On demande le bruit à ces coordonnées 3D
                float n = noise.GetNoise(nx, ny, nz);
                
                // Normalisation et relief
                n = (n + 1.0f) / 2.0f;
                if (n < 0.5f) {
                    n = std::pow(n, 1.5f) * 0.8f; 
                } else {
                    n = 0.4f + std::pow(n - 0.5f, 0.7f) * 1.2f; 
                }

                heightMap[y * mapWidth + x] = n;
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
            glm::vec3 n = glm::normalize(p) * this->radius;
            vertices.push_back({n, 1.0f, 0.0f, 0.0f, glm::normalize(n)});
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
        this->relativeStrength = strength;
        for (auto& v : vertices) {
            calculateUV(v);

            // Échantillonne la heightMap avec interpolation bilinéaire
            float noiseValue = sampleHeightMap(v.u, v.v);

            float displacement;
            if (noiseValue < 0.5f) {
                displacement = 1.0f;
            } else {
                displacement = 1.0f + (noiseValue - 0.5f) * strength;
            }

            // Appliquer le rayon correct (radius * displacement)
            v.position = glm::normalize(v.position) * (this->radius * displacement);
            v.altitude = noiseValue;
        }
        // Indispensable pour que la lumière soit correcte sur le nouveau relief
        calculateNormals();
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