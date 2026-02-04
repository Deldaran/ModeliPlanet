#version 330 core
out vec4 FragColor;

in vec3 FragPos;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 planetCenter;
uniform float time;
uniform float cloudRotationOffset;

// Cloud Parameters
uniform float planetRadius; 
uniform float cloudMinHeight; 
uniform float cloudMaxHeight; 

// Textures
uniform sampler2D cloudCoverageMap; // La texture de la planète
uniform sampler3D noiseTexture3D;   // Bruit volumétrique

const int STEPS = 64;

// --- UTILS ---
float remap(float v, float minOld, float maxOld, float minNew, float maxNew) {
    return minNew + (v - minOld) * (maxNew - minNew) / (maxOld - minOld);
}

float beer(float d) { return exp(-d); }
float henyeyGreenstein(float g, float cosTheta) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159 * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}

// Intersect Sphere Function
vec2 intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return vec2(-1.0);
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

float sampleCloudDensity(vec3 p) {
    float len = length(p - planetCenter);
    float hFrac = (len - (planetRadius + cloudMinHeight)) / (cloudMaxHeight - cloudMinHeight);
    if(hFrac < 0.0 || hFrac > 1.0) return 0.0;

    // --- 1. COVERAGE (BASE SHAPE) ---
    // Au lieu de dépendre uniquement de la texture 2D qui a des seams, on mixe avec du bruit 3D
    // Si la texture 2D est moche/vide, le bruit 3D sauvera l'affaire.
    
    // UV Mapping (Spherical) pour la map 2D
    vec3 dir = normalize(p - planetCenter);
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * 3.14159);
    float v = 0.5 - asin(dir.y) / 3.14159;
    
    // Low Frequency 3D Noise (The "Continent" shapes)
    // Scale must be carefully chosen. Too small = cuts visible if not perfect.
    // Increased scale to 0.001 to have more cloud systems instead of one big blob
    vec3 animPos = p * 0.001 + vec3(time * 0.001, 0.0, 0.0);
    float lowFreq = texture(noiseTexture3D, animPos).r; 

    // USE 2D MAP for GLOBAL COVERAGE (To create continents of clouds)
    // But mask seams by fading out at poles or blending?
    // Let's just use the map to decide WHERE clouds are allowed.
    vec2 uv = vec2(u + cloudRotationOffset, v);
    float mapCoverage = texture(cloudCoverageMap, uv).r; 

    // Combine:
    // If mapCoverage is low, no clouds.
    // If mapCoverage is high, we let the noise decide the shape.
    // This breaks the "Noise texture pattern" look.
    float coverage = lowFreq * mapCoverage; 

    // Thresholding
    // Increased Min threshold from 0.2 to 0.5 to cut out more clouds (make holes)
    coverage = remap(coverage, 0.55, 1.0, 0.0, 1.0); 
    if (coverage <= 0.0) return 0.0;

    // --- 2. DETAIL EROSION ---
    // High Frequency Noise
    // On déplace le détail un peu plus vite pour simuler le vent interne
    vec3 detailPos = p * 0.001 + vec3(time * 0.005, 0.0, 0.0);
    vec4 highFreq = texture(noiseTexture3D, detailPos);
    float erosion = highFreq.g * 0.5 + highFreq.b * 0.25 + highFreq.a * 0.125;
    
    // On érode la forme de base
    float cloud = remap(coverage, erosion * 0.3, 1.0, 0.0, 1.0);
    
    // --- 3. DENSITY GRADIENT ---
    // Arrondir le haut et le bas
    float heightCurve = smoothstep(0.0, 0.2, hFrac) * smoothstep(1.0, 0.6, hFrac);
    
    return cloud * heightCurve * 2.0; // Density boost
}

// Light march (simple shadow check)
float getLight(vec3 p, vec3 lightDir) {
    float dist = 20.0; // Distance to check towards sun
    vec3 pL = p + lightDir * dist;
    // Un seul sample pour la performance (mais lointain)
    float dens = sampleCloudDensity(pL);
    return exp(-dens * 2.0); // Beer's law shadow
}

void main() {
    vec3 rayDir = normalize(FragPos - viewPos);
    vec3 ro = viewPos;
    
    float minR = planetRadius + cloudMinHeight;
    float maxR = planetRadius + cloudMaxHeight;

    // Intersections
    vec2 hitOut = intersectSphere(ro, rayDir, planetCenter, maxR);
    float tNear = hitOut.x;
    float tFar = hitOut.y;
    
    if (tFar < 0.0) discard;
    
    vec2 hitIn = intersectSphere(ro, rayDir, planetCenter, minR);
    
    // Logique bounds
    float tStart = max(0.0, tNear);
    float tEnd = tFar;
    
    // CAS SPECIAL: Camera sous les nuages
    if (length(ro - planetCenter) < minR) {
        tStart = hitIn.y;
        tEnd = hitOut.y;
    }
    
    // On ne clippe PLUS le ray avec hitIn.x (le sol).
    // On laisse le raymarching traverser le vide interne si besoin.
    // sampleCloudDensity renverra 0 si on est en dessous de minR.
    // Cela évite les artefacts visuels d'intersection (le "Dome") à l'horizon.
    
    if (tEnd <= tStart) discard;
    
    // Limite distance visuelle
    if ((tEnd - tStart) > 100000.0) tEnd = tStart + 100000.0;

    // Raymarch
    int steps = STEPS;
    float stepSize = (tEnd - tStart) / float(steps);
    vec3 p = ro + rayDir * tStart;
    
    vec3 lightDir = normalize(lightPos - planetCenter);
    float cosTheta = dot(rayDir, lightDir);
    float phase = henyeyGreenstein(0.6, cosTheta);
    
    // Dithering
    p += rayDir * stepSize * fract(sin(dot(gl_FragCoord.xy, vec2(12.9898,78.233))) * 43758.5453);

    vec4 sum = vec4(0.0);
    float transmittance = 1.0;

    for(int i=0; i<steps; i++) {
        if(transmittance < 0.01) break;
        
        float density = sampleCloudDensity(p);
        
        if(density > 0.0) {
            float stepDens = density * stepSize * 0.2; // Scale density
            float lightTrans = getLight(p, lightDir);
            
            // Powder effect (darker edges) approx
            float powder = 1.0 - exp(-stepDens * 2.0);
            
            // Lighting energy
            float energy = lightTrans * phase * powder + 0.1; // + Ambient
            
            vec3 col = vec3(energy); // Nuages blancs
            
            float alpha = 1.0 - beer(stepDens);
            sum.rgb += col * alpha * transmittance;
            transmittance *= (1.0 - alpha);
        }
        
        p += rayDir * stepSize;
    }

    if(transmittance >= 0.99) discard;

    // Output
    FragColor = vec4(sum.rgb, 1.0 - transmittance);
}
