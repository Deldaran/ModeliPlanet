#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "../src/FastNoiseLite.h" 
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <omp.h> 

const int WIDTH = 4096;
const int HEIGHT = 2048; 
const float PI = 3.14159265359f;

void uvToSphere(float u, float v, float& x, float& y, float& z) {
    float theta = 2.0f * PI * u;
    float phi = PI * (1.0f - v);
    x = sin(phi) * cos(theta);
    y = cos(phi);
    z = sin(phi) * sin(theta);
}

// Implementation standard de smoothstep pour C++
float smoothstep(float edge0, float edge1, float x) {
    x = std::min(std::max((x - edge0) / (edge1 - edge0), 0.0f), 1.0f); 
    return x * x * (3 - 2 * x);
}

int main() {
    std::cout << "--- GENERATEUR DE NUAGES ---" << std::endl;
    std::cout << "Creation de la Cloud Map 4K..." << std::endl;

    // 1. Noise spécial "Nuages"
    FastNoiseLite cloudNoise;
    cloudNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    cloudNoise.SetFractalType(FastNoiseLite::FractalType_FBm); // Billow est bien aussi, mais FBm est plus controllé
    cloudNoise.SetFractalOctaves(5);
    cloudNoise.SetFrequency(1.5f); // Assez gros
    cloudNoise.SetFractalGain(0.5f);
    cloudNoise.SetFractalLacunarity(2.0f);

    // 2. Noise de "Coupure" (pour casser les répétitions et faire des zones vides)
    FastNoiseLite maskNoise;
    maskNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    maskNoise.SetFrequency(0.5f); // Tres large
    
    std::vector<unsigned char> pixels(WIDTH * HEIGHT * 4); // RGBA pour la transparence

    #pragma omp parallel for
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            float u = (float)x / (float)WIDTH;
            float v = (float)y / (float)HEIGHT;

            float nx, ny, nz;
            uvToSphere(u, v, nx, ny, nz);

            // Génération
            float base = cloudNoise.GetNoise(nx, ny, nz); // -1 à 1
            float mask = maskNoise.GetNoise(nx + 10.0f, ny, nz); // Offset pour ne pas matcher

            // Traitement
            float density = base + mask * 0.5f; // Mélange
            
            // Contraste fort pour des bords nets
            float finalVal = (density + 0.2f); 
            if(finalVal < 0.0f) finalVal = 0.0f;
            finalVal = std::pow(finalVal, 3.0f) * 4.0f; // Boost
            finalVal = std::min(finalVal, 1.0f);
            
            // Adoucir les bords légèrement
            finalVal = smoothstep(0.1f, 0.9f, finalVal);

            unsigned char val = static_cast<unsigned char>(finalVal * 255);
            
            int index = (y * WIDTH + x) * 4;
            // Nuage Blanc
            pixels[index]   = 255; 
            pixels[index+1] = 255;
            pixels[index+2] = 255;
            // Alpha = Densité
            pixels[index+3] = val; 
        }
    }

    stbi_write_png("planet_clouds.png", WIDTH, HEIGHT, 4, pixels.data(), WIDTH * 4);
    std::cout << "Termine ! 'planet_clouds.png' cree." << std::endl;
    return 0;
}
