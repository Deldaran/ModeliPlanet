#version 330 core
out vec4 FragColor;
in vec3 FragPos;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 planetCenter;
uniform float time;
uniform float planetRadius; 
uniform float cloudMinHeight; 
uniform float cloudMaxHeight; 
uniform float cloudRotationAngle;

// NOUVEAU: Pour la lecture de profondeur (Occlusion réaliste)
uniform sampler2D depthMap;
uniform vec2 screenSize;
uniform mat4 invView;
uniform mat4 invProj;

uniform sampler3D noiseTexture3D; 

const int STEPS = 64; 
const float PI = 3.14159265;

// --- RECONSTRUCTION POSITION MONDE DEPUIS DEPTH MAP ---
vec3 WorldPosFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    vec2 uv = gl_FragCoord.xy / screenSize;
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = invProj * clipSpacePosition;
    
    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;
    
    vec4 worldSpacePosition = invView * viewSpacePosition;
    return worldSpacePosition.xyz;
}

// --- OUTILS MATHÉMATIQUES ---
float remap(float x, float a, float b, float c, float d) {
    return c + (d - c) * (x - a) / (b - a);
}

vec3 rotateY(vec3 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec3(p.x * c - p.z * s, p.y, p.x * s + p.z * c);
}

vec2 intersectSphereFull(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return vec2(-1.0, -1.0);
    float sq = sqrt(h);
    return vec2(-b - sq, -b + sq);
}

float intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return -1.0;
    return -b - sqrt(h);
}

// --- DENSITÉ FLUIDE ---
float sampleCloudDensity(vec3 p) {
    float driftSpeed  = 0.00005;
    float morphSpeed  = 0.00025;
    float detailSpeed = 0.0001;

    vec3 relPos = p - planetCenter;
    float dist = length(relPos);
    float hFrac = (dist - (planetRadius + cloudMinHeight)) / (cloudMaxHeight - cloudMinHeight);
    
    if(hFrac < 0.0 || hFrac > 1.0) return 0.0;

    float lat = relPos.y / planetRadius;
    float windSpeed = cloudRotationAngle * (1.0 + cos(lat * PI) * 0.2);
    
    vec3 baseRotation = rotateY(relPos, windSpeed);
    vec3 distortion = texture(noiseTexture3D, baseRotation * 0.00005 + time * driftSpeed).rgb * 0.1;
    vec3 pFluid = baseRotation + (distortion * planetRadius * 0.1);

    vec3 weatherCoord = pFluid * 0.000008;
    vec4 weatherSample = texture(noiseTexture3D, weatherCoord);
    float moisture = smoothstep(0.4, 0.6, weatherSample.r);
    
    if (moisture <= 0.01) return 0.0;

    float lifeCycle = texture(noiseTexture3D, pFluid * 0.0001 + time * morphSpeed).b;
    float threshold = mix(0.4, 0.7, lifeCycle);

    float detailNoise = texture(noiseTexture3D, pFluid * 0.0004 + vec3(0.0, time * detailSpeed, 0.0)).r;

    float density = smoothstep(threshold, threshold + 0.1, detailNoise);
    density *= moisture;

    float erosion = texture(noiseTexture3D, pFluid * 0.001).a * 0.5 * hFrac;
    density = max(0.0, density - erosion);

    density *= smoothstep(0.0, 0.15, hFrac) * smoothstep(1.0, 0.6, hFrac);

    // Retour à une densité plus naturelle (ni fantôme, ni brique)
    return density * 20.0; 
}

float getLighting(vec3 p, vec3 lightDir) {
    float d = 0.0;
    float stepL = 10.0; 
    for(int i=0; i<6; i++) {
        vec3 sp = p + lightDir * stepL * float(i+1);
        d += sampleCloudDensity(sp) * 0.5;
    }
    float beer = exp(-d * 0.3);
    float powder = 1.0 - exp(-d * 1.0);
    return beer * powder * 2.5;
}

void main() {
    vec3 rayDir = normalize(FragPos - viewPos);
    vec3 lightDir = normalize(lightPos - planetCenter); 
    
    float rMin = planetRadius + cloudMinHeight;
    float rMax = planetRadius + cloudMaxHeight;
    vec3 oc = viewPos - planetCenter;
    float distCam = length(oc);

    // --- INTERSECTION AVEC LES DEUX SPHÈRES (couche interne & externe) ---
    vec2 hitOuter = intersectSphereFull(viewPos, rayDir, planetCenter, rMax);
    vec2 hitInner = intersectSphereFull(viewPos, rayDir, planetCenter, rMin);
    
    // Pas d'intersection avec la couche extérieure => pas de nuage visible
    if (hitOuter.x < 0.0 && hitOuter.y < 0.0) discard;

    float tStart, tEnd;
    
    if (distCam > rMax) {
        // --- CAMÉRA AU-DESSUS DES NUAGES (ESPACE) ---
        tStart = hitOuter.x;
        if (hitInner.x > 0.0) {
            tEnd = hitInner.x; // Arrêt à la sphère intérieure
        } else {
            tEnd = hitOuter.y;
        }
    } else if (distCam < rMin) {
        // --- CAMÉRA EN DESSOUS DES NUAGES (SOL) ---
        // Le rayon part de la caméra, traverse l'air libre, puis entre dans la couche
        // par la sphère intérieure (hitInner.y = sortie = point d'entrée vu du dessous)
        if (hitInner.y > 0.0) {
            tStart = hitInner.y;
        } else {
            // La caméra est sous rMin mais le rayon ne touche pas la sphère interne 
            // (regarde à l'horizontale loin) -> commence au point le plus proche de la couche
            tStart = 0.0;
        }
        tEnd = hitOuter.y;
    } else {
        // --- CAMÉRA DANS LES NUAGES ---
        tStart = 0.0;
        tEnd = hitOuter.y;
    }

    // --- OCCLUSION PAR LA PROFONDEUR DE LA SCÈNE (REALISME MAXIMAL) ---
    // Au lieu de deviner la sphère, on lit le Depth Buffer réel (Montagnes, Objets)
    float zDepth = texture(depthMap, gl_FragCoord.xy / screenSize).r;
    float distGeometry = 1e20; // Infini
    
    if (zDepth < 1.0) {
        vec3 worldPosGeom = WorldPosFromDepth(zDepth);
        distGeometry = length(worldPosGeom - viewPos);
    }

    // 1. Si l'objet solide est PLUS PROCHE que le début du nuage, le nuage est caché.
    if (distGeometry < tStart) {
        discard;
    }

    // 2. Si le rayon entre dans le nuage, on l'arrête dès qu'il touche un obstacle solide.
    // Cela "coupe" le nuage exactement selon la forme des montagnes.
    if (distGeometry < tEnd) {
        tEnd = distGeometry;
    }

    // Sécurité
    tStart = max(0.0, tStart);
    float totalDist = tEnd - tStart;
    if (totalDist <= 0.0) discard;

    float stepSize = totalDist / float(STEPS);
    float jitter = fract(sin(gl_FragCoord.x * 12.9898 + gl_FragCoord.y * 78.233) * 43758.5453) * stepSize;
    vec3 p = viewPos + rayDir * (tStart + jitter);
    
    vec4 res = vec4(0.0);
    float T = 1.0;

    for(int i=0; i<STEPS; i++) {
        float dens = sampleCloudDensity(p);
        
        if(dens > 0.01) {
            float hitPl = intersectSphere(p, lightDir, planetCenter, planetRadius);
            float shadow = (hitPl > 0.0) ? 0.0 : 1.0; 

            float sunAltitude = dot(normalize(p - planetCenter), lightDir);
            vec3 sunCol = vec3(1.4, 1.3, 1.2); 
            if(sunAltitude < 0.3) {
                float sunsetFactor = clamp(remap(sunAltitude, -0.1, 0.3, 0.0, 1.0), 0.0, 1.0);
                sunCol = mix(vec3(1.6, 0.5, 0.2), sunCol, sunsetFactor);
            }
            
            float li = getLighting(p, lightDir) * shadow;
            float cosTheta = dot(rayDir, lightDir);
            float phase = mix(0.2, 1.5, pow(max(0.0, cosTheta), 8.0));
            vec3 ambCol = vec3(0.2, 0.3, 0.5) * (shadow * 0.6 + 0.1); 
            
            vec3 finalColor = (sunCol * li * phase) + ambCol;
            
            // Coefficient d'extinction : 0.35 (Equilibre entre transparence et opacité)
            // Assez opaque pour cacher la planète, assez transparent pour voir du volume
            float alpha = (1.0 - exp(-dens * stepSize * 0.35));
            
            // Pré-multiplié : Color * Alpha * Transmittance
            res.rgb += finalColor * alpha * T;
            T *= (1.0 - alpha);
            if(T < 0.01) break;
        }
        p += rayDir * stepSize;
    }

    // Pas de nuage traversé => rien à dessiner
    if (T > 0.99) discard;

    // --- SORTIE PRÉ-MULTIPLIÉE ---
    // Blend mode CPU : GL_ONE, GL_ONE_MINUS_SRC_ALPHA
    // res.rgb est déjà pré-multiplié dans la boucle
    float finalAlpha = 1.0 - T;

    FragColor = vec4(res.rgb, finalAlpha);
}
