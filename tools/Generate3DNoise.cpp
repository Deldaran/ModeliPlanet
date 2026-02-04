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

    // 1. Setup Noises
    // Channel R: Perlin-Worley construction
    // For simplicity here, we will put Perlin FBM in R.
    FastNoiseLite perlinNoise;
    perlinNoise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    perlinNoise.SetFrequency(0.02f);
    perlinNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
    perlinNoise.SetFractalOctaves(3);

    // Channel G: Worley (Cellular) Low Freq
    FastNoiseLite worleyLow;
    worleyLow.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worleyLow.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worleyLow.SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);
    worleyLow.SetFrequency(0.04f); // More frequent than perlin

    // Channel B: Worley Medium Freq
    FastNoiseLite worleyMed;
    worleyMed.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worleyMed.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worleyMed.SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);
    worleyMed.SetFrequency(0.08f);

    // Channel A: Worley High Freq
    FastNoiseLite worleyHigh;
    worleyHigh.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    worleyHigh.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Euclidean);
    worleyHigh.SetCellularReturnType(FastNoiseLite::CellularReturnType_Distance);
    worleyHigh.SetFrequency(0.16f);

    // Buffer: RGBA (4 bytes per voxel)
    std::vector<unsigned char> data(SIZE * SIZE * SIZE * 4);

    #pragma omp parallel for collapse(3)
    for (int z = 0; z < SIZE; ++z) {
        for (int y = 0; y < SIZE; ++y) {
            for (int x = 0; x < SIZE; ++x) {
                // Determine 3D coordinate (tileable preferred, but let's stick to simple coords first)
                // Note: FastNoiseLite is not strictly tileable out of the box without domain warping trickery,
                // but for a planetary scale cloud system, simplified non-tiled is often okay if UVs wrap or we rely on sphere mapping.
                // However, valid scrolling clouds usually require tiling noise. 
                // Creating TILEABLE noise with FastNoiseLite is tricky.
                // We will assume no tiling (clamping) or simple large scale noise for now.
                
                float fx = (float)x;
                float fy = (float)y;
                float fz = (float)z;

                // --- R: PERLIN-WORLEY BASE ---
                // "Perlin-Worley" usually means Perlin * (1 - Worley) to erode it.
                // Let's just store Pure Perlin in R for now, and Worley in G/B/A.
                float p = perlinNoise.GetNoise(fx, fy, fz); // -1 to 1
                p = (p + 1.0f) * 0.5f; // 0 to 1
                
                // --- G, B, A: WORLEY FBM ---
                // Cellular noise returns -1 to 1 usually in FNL depending on settings, 
                // but Distance return type is strictly 0 to ~1+ range usually.
                // We need to invert it because Worley centers are holes (0 distance). 
                // Clouds should be dense in centers? 
                // Actually in typical textures, Worley noise is used as "invert distance" so center is 1 (dense) and edge is 0.
                
                float w1 = worleyLow.GetNoise(fx, fy, fz); // -1 to 1 range typically for FNL even for cellular?
                // Actually FNL cellular distance is often 0 to 1+
                // Let's normalize assuming simple range or check FNL docs mentally. 
                // FNL GetNoise always returns -1..1 range approx.
                // For Cellular Distance, -1 is closest, 1 is farthest? No.
                // Let's try standard mapping `(n+1)/2`.
                w1 = 1.0f - std::abs(w1); // Center of cell (0) becomes 1. Edge becomes 0.
                
                float w2 = worleyMed.GetNoise(fx, fy, fz);
                w2 = 1.0f - std::abs(w2);

                float w3 = worleyHigh.GetNoise(fx, fy, fz);
                w3 = 1.0f - std::abs(w3);

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
