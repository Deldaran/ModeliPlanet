#define _CRT_SECURE_NO_WARNINGS
#include "../src/FastNoiseLite.h" 
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <omp.h> 

const int SIZE = 128; // 128x128x128 Volume

// Helper to remap value from one range to another
float remap(float value, float originalMin, float originalMax, float newMin, float newMax) {
    return newMin + (value - originalMin) * (newMax - newMin) / (originalMax - originalMin);
}

// Invert noise: 1.0 - value
float invert(float n) {
    return 1.0f - n;
}

int main() {
    std::cout << "--- 3D NOISE GENERATOR (Blackrack Style) ---" << std::endl;
    std::cout << "Generating " << SIZE << "x" << SIZE << "x" << SIZE << " texture..." << std::endl;

    // --- 1. CONFIGURATION DES BRUITS ---
    // R: Perlin-Worley Base (Structure nuageuse cotonneuse)
    FastNoiseLite perlinNoise;
    perlinNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    perlinNoise.SetFrequency(4.0f); // Structure globale (Base frequency relative to unit box)
    perlinNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    perlinNoise.SetFractalOctaves(4);
    perlinNoise.SetFractalLacunarity(2.0f);
    perlinNoise.SetFractalGain(0.5f);

    // G: Worley Coarse (Erosion basse frequence)
    FastNoiseLite worleyLow;
    worleyLow.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worleyLow.SetFrequency(4.0f);
    worleyLow.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worleyLow.SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);

    // B: Worley Medium (Detail erosion)
    FastNoiseLite worleyMed;
    worleyMed.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worleyMed.SetFrequency(8.0f);
    worleyMed.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worleyMed.SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);

    // A: Worley Fine (Micro details)
    FastNoiseLite worleyHigh;
    worleyHigh.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worleyHigh.SetFrequency(16.0f);
    worleyHigh.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worleyHigh.SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);

    // Buffer: RGBA
    std::vector<unsigned char> data(SIZE * SIZE * SIZE * 4);

    #pragma omp parallel for collapse(3)
    for (int z = 0; z < SIZE; ++z) {
        for (int y = 0; y < SIZE; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                // Coordonnees normalisees [0, 1]
                float fx = (float)x / (float)SIZE;
                float fy = (float)y / (float)SIZE;
                float fz = (float)z / (float)SIZE;

                // Tiling frequency wrapper for FastNoiseLite is complex without Domain Warping.
                // Ici on suppose que le sampler3D en mode "GL_REPEAT" ou "GL_MIRRORED_REPEAT" fera l'affaire.
                // On multiplie par la frequence désirée directement dans les settings noise au dessus.
                
                // --- R: PERLIN FBM ---
                float p = perlinNoise.GetNoise(fx * SIZE, fy * SIZE, fz * SIZE); 
                // Map [-1, 1] -> [0, 1]
                float rVal = (p + 1.0f) * 0.5f;

                // --- G: WORLEY LOW ---
                // FNL Cellular Distance: 0 (center) to ~1 (edge).
                // Inverse to get blobs: 1 (center) to 0 (edge).
                float w1 = worleyLow.GetNoise(fx * SIZE, fy * SIZE, fz * SIZE);
                w1 = 1.0f - std::min(std::abs(w1), 1.0f); // Inverted Worley

                // --- B: WORLEY MED ---
                float w2 = worleyMed.GetNoise(fx * SIZE, fy * SIZE, fz * SIZE);
                w2 = 1.0f - std::min(std::abs(w2), 1.0f);

                // --- A: WORLEY HIGH ---
                float w3 = worleyHigh.GetNoise(fx * SIZE, fy * SIZE, fz * SIZE);
                w3 = 1.0f - std::min(std::abs(w3), 1.0f);


                // --- Combine / Store ---
                int idx = (z * SIZE * SIZE + y * SIZE + x) * 4;
                
                data[idx + 0] = (unsigned char)(p * 255.0f);   // R: Base Shape
                data[idx + 1] = (unsigned char)(w1 * 255.0f);  // G: Coarse Detail
                data[idx + 2] = (unsigned char)(w2 * 255.0f);  // B: Medium Detail
                data[idx + 3] = (unsigned char)(w3 * 255.0f);  // A: Fine Detail
            }
        }
    }

    // Write to binary file
    std::string filename = "assets/noise_shape_128.bin";
    std::ofstream outfile(filename, std::ios::out | std::ios::binary);
    if (!outfile) {
        std::cerr << "Error opening " << filename << " for writing." << std::endl;
        return 1;
    }
    outfile.write((char*)data.data(), data.size());
    outfile.close();

    std::cout << "Done! Saved to " << filename << " (" << data.size() / (1024*1024) << " MB)" << std::endl;
    return 0;
}
