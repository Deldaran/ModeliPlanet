#version 330 core
out vec4 FragColor;

in float vAltitude;
in vec2 vTexCoords;
in vec3 vNormal;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 planetCenter;
uniform float planetRadius;
uniform float cloudMinHeight;
uniform float cloudMaxHeight;
uniform float cloudRotationAngle;
uniform float time;

uniform sampler2D shadowMap;
uniform sampler2D planetData;
uniform sampler3D noiseTexture3D;

const float PI = 3.14159265;

float remap(float x, float a, float b, float c, float d) {
    return c + (d - c) * (x - a) / (b - a);
}

vec3 rotateY(vec3 p, float angle) {
    float s = sin(angle); float c = cos(angle);
    return vec3(p.x * c - p.z * s, p.y, p.x * s + p.z * c);
}

float getCloudDensityForShadow(vec3 p) {
    float driftSpeed = 0.00005; float morphSpeed = 0.00025;
    vec3 relPos = p - planetCenter;
    float dist = length(relPos);
    float hFrac = (dist - (planetRadius + cloudMinHeight)) / (cloudMaxHeight - cloudMinHeight);
    if(hFrac < 0.0 || hFrac > 1.0) return 0.0;

    float lat = relPos.y / planetRadius;
    float windSpeed = cloudRotationAngle * (1.0 + cos(lat * PI) * 0.2);
    vec3 baseRotation = rotateY(relPos, windSpeed);
    vec3 pFluid = baseRotation + (texture(noiseTexture3D, baseRotation * 0.00005 + time * driftSpeed).rgb * 0.1 * planetRadius * 0.1);

    float moisture = smoothstep(0.4, 0.6, texture(noiseTexture3D, pFluid * 0.000008).r);
    if (moisture <= 0.01) return 0.0;

    float lifeCycle = texture(noiseTexture3D, pFluid * 0.0001 + time * morphSpeed).b;
    float threshold = mix(0.4, 0.7, lifeCycle);
    float density = smoothstep(threshold, threshold + 0.1, texture(noiseTexture3D, pFluid * 0.0004 + vec3(0.0, time * 0.0001, 0.0)).r);

    return density * moisture * smoothstep(0.0, 0.1, hFrac) * smoothstep(1.0, 0.5, hFrac);
}

float getCloudShadow(vec3 fragPos, vec3 lightDir) {
    float shadowAcc = 0.0;
    for(int i = 0; i < 4; i++) {
        shadowAcc += getCloudDensityForShadow(fragPos + lightDir * (cloudMinHeight + float(i) * (cloudMaxHeight - cloudMinHeight) / 4.0));
    }
    return clamp(shadowAcc * 0.5, 0.0, 0.75);
}

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vFragPos);
    vec3 planetN = normalize(vFragPos - planetCenter);
    float sunAltitude = dot(planetN, lightDir);

    float cloudShadow = getCloudShadow(vFragPos, lightDir);
    
    vec3 sunCol = vec3(1.5, 1.4, 1.3);
    if (sunAltitude < 0.25) {
        sunCol = mix(vec3(1.6, 0.4, 0.1), sunCol, clamp(remap(sunAltitude, -0.1, 0.25, 0.0, 1.0), 0.0, 1.0));
    }

    float heightHD = texture(planetData, vTexCoords).r;
    float grain = texture(noiseTexture3D, vec3(vTexCoords * 15.0, time * 0.01)).r;
    float a_detailed = heightHD + (grain * 0.015 - 0.007);

    // --- REINTEGRATION DES BIOMES ORIGINAUX ---
    vec3 color;
    if (a_detailed < 0.5) {
        color = mix(vec3(0.005, 0.01, 0.2), vec3(0.0, 0.35, 0.5), clamp(a_detailed * 2.0, 0.0, 1.0));
    } else {
        float h = (a_detailed - 0.5) * 2.0;
        if (h < 0.03) color = vec3(0.76, 0.70, 0.50); // Beach
        else if (h < 0.35) {
            color = mix(vec3(0.2, 0.4, 0.15), vec3(0.05, 0.25, 0.05), smoothstep(0.03, 0.35, h)); // Plain/Forest
        } else if (h < 0.65) {
            color = mix(vec3(0.05, 0.25, 0.05), vec3(0.35, 0.32, 0.30), smoothstep(0.35, 0.65, h)); // Rock
        } else {
            color = mix(vec3(0.35, 0.32, 0.30), vec3(0.95, 0.95, 1.0), smoothstep(0.65, 0.85, h)); // Snow
        }
    }

    float totalShadow = max(cloudShadow, smoothstep(0.05, -0.05, sunAltitude));
    vec3 lighting = (0.05 * color) + (1.0 - totalShadow) * (max(dot(norm, lightDir), 0.0) * color * sunCol);

    // BROUILLARD ATMOSPHÉRIQUE (distance seulement)
    // Léger fog à longue distance pour simuler la diffusion atmosphérique
    float fragDist = length(viewPos - vFragPos);
    float camAltitude = length(viewPos - planetCenter) - planetRadius;
    // Le fog n'apparait que si on est dans l'atmosphère (< 300 unités d'altitude)
    // et uniquement sur les objets lointains
    if (camAltitude < 300.0) {
        float maxFogDist = 800.0 + camAltitude * 5.0; // Plus on est haut, plus on voit loin
        float fogFactor = smoothstep(maxFogDist * 0.3, maxFogDist, fragDist) * 0.4;
        vec3 fogColor = vec3(0.55, 0.6, 0.7);
        lighting = mix(lighting, fogColor, fogFactor);
    }
    FragColor = vec4(lighting, 1.0);
}