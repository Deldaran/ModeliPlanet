#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "../src/FastNoiseLite.h" 
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <omp.h> // Si le compilateur le supporte, sinon ignorer

// --- CONFIGURATION ---
const int WIDTH = 4096;  // Haute résolution
const int HEIGHT = 2048; 
const float PI = 3.14159265359f;

// --- UTILS ---
// Implementation standard de smoothstep pour C++
float smoothstep(float edge0, float edge1, float x) {
    // Scale, bias and saturate x to 0..1 range
    x = std::min(std::max((x - edge0) / (edge1 - edge0), 0.0f), 1.0f); 
    // Evaluate polynomial
    return x * x * (3 - 2 * x);
}

void uvToSphere(float u, float v, float& x, float& y, float& z) {
    float theta = 2.0f * PI * u;     // Longitude (0 -> 2PI)
    float phi = PI * (1.0f - v);     // Latitude (0 -> PI)
    x = sin(phi) * cos(theta);
    y = cos(phi);
    z = sin(phi) * sin(theta);
}

// Fonction de bruit composite pour des détails réalistes
float getPlanetNoise(FastNoiseLite& baseNoise, FastNoiseLite& mountainNoise, FastNoiseLite& detailNoise, float x, float y, float z) {
    // 1. Continents de base (Grandes formes douces)
    float base = baseNoise.GetNoise(x, y, z); // -1 à 1
    
    // 2. Montagnes (Bruit "Ridged" pour des pics acérés) - on masque par les continents
    float ridges = mountainNoise.GetNoise(x, y, z); // -1 à 1
    ridges = std::abs(ridges); // Crêtes
    ridges = -ridges + 1.0f;   // Inverser pour avoir des pics
    ridges = pow(ridges, 3.0f); // Accentuer les pics

    // 3. Détails fins (Haute fréquence)
    float details = detailNoise.GetNoise(x, y, z) * 0.1f;

    // --- MIXAGE ---
    // Si base < 0, c'est l'océan. On aplatit.
    float blendFactor = smoothstep(-0.1f, 0.2f, base); // Transition douce océan/terre
    
    float continentHeight = (base + 0.5f) * 0.3f; // Base élévation
    float mountainHeight = ridges * 0.8f * blendFactor; // Montagnes seulement sur terre
    
    // Résultat combiné
    float finalH = continentHeight + mountainHeight + (details * blendFactor);
    
    // Clamp pour rester gérable (0.0 = fond marin, 1.0 = pic Himalaya)
    // On remonte un peu le niveau de la mer artificiellement pour la texture
    return finalH; 
}

int main() {
    std::cout << "--- GENERATEUR DE PLANETE HD ---" << std::endl;
    std::cout << "Resolution: " << WIDTH << "x" << HEIGHT << std::endl;

    // 1. Configuration des Bruits
    FastNoiseLite baseNoise; 
    baseNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    baseNoise.SetFrequency(1.0f); // Formes très larges (Continents)
    baseNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    baseNoise.SetFractalOctaves(4);

    FastNoiseLite mountainNoise;
    mountainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    mountainNoise.SetFrequency(3.0f); // Plus serré
    mountainNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    mountainNoise.SetFractalOctaves(6); // Beaucoup de détails dans les montagnes

    FastNoiseLite detailNoise;
    detailNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    detailNoise.SetFrequency(12.0f); // Très haute fréquence (Rugosité)
    detailNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    detailNoise.SetFractalOctaves(4);

    // 2. Buffer Image
    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 3);

    // 3. Boucle principale (Optimisation possible avec OpenMP si dispo)
    int counter = 0;
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            float u = (float)x / (float)WIDTH;
            float v = (float)y / (float)HEIGHT;

            // Coordonnées Sphériques 3D
            float nx, ny, nz;
            uvToSphere(u, v, nx, ny, nz);

            // Calcul de l'altitude complexe
            float h = getPlanetNoise(baseNoise, mountainNoise, detailNoise, nx, ny, nz);

            // Normalisation pour le PNG (0-255)
            // On map arbitrairement -0.5 (fond) à 1.5 (pic) vers 0-255
            float normalized = (h + 0.5f) / 2.0f; 
            normalized = std::max(0.0f, std::min(1.0f, normalized));

            unsigned char val = static_cast<unsigned char>(normalized * 255);

            int index = (y * WIDTH + x) * 3;
            // On écrit une Heightmap en Grayscale (R=G=B)
            // Plus tard, on pourra encoder la précision sur les 3 canaux si besoin (RGB packed)
            pixels[index]   = val;
            pixels[index+1] = val;
            pixels[index+2] = val;
        }
        if (y % 100 == 0) {
            std::cout << "Ligne " << y << "/" << HEIGHT << "\r";
        }
    }

    std::cout << "\nEcriture du fichier disque..." << std::endl;
    stbi_write_png("planet_heightmap_hd.png", WIDTH, HEIGHT, 3, pixels.data(), WIDTH * 3);
    
    std::cout << "Termine ! 'planet_heightmap_hd.png' cree." << std::endl;
    return 0;
}
