#version 330 core
out vec4 FragColor;

in vec3 FragPos;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 planetCenter = vec3(0.0); // Simplification, le centre est souvent 0,0,0 localement
uniform float time;
uniform float cloudRotationOffset; // Sync avec la planete

// Parametres Nuages Volumetriques
uniform float planetRadius; 
uniform float cloudMinHeight; // ex: 2000
uniform float cloudMaxHeight; // ex: 4000

uniform sampler2D cloudCoverageMap; // La texture planet_clouds.png

// --- NOISE FUNCTIONS (Simplex 3D Cheap) ---
// Source: https://www.shadertoy.com/view/XsX3zB
vec3 random3(vec3 c) {
    float j = 4096.0*sin(dot(c,vec3(17.0, 59.4, 15.0)));
    vec3 r;
    r.z = fract(512.0*j);
    j *= .125;
    r.x = fract(512.0*j);
    j *= .125;
    r.y = fract(512.0*j);
    return r-0.5;
}

const float F3 =  0.3333333;
const float G3 =  0.1666667;

float snoise(vec3 p) {
    vec3 s = floor(p + dot(p, vec3(F3)));
    vec3 x = p - s + dot(s, vec3(G3));
    vec3 e = step(vec3(0.0), x - x.yzx);
    vec3 i1 = e*(1.0 - e.zxy);
    vec3 i2 = 1.0 - e.zxy*(1.0 - e);
    vec3 x1 = x - i1 + G3;
    vec3 x2 = x - i2 + 2.0*G3;
    vec3 x3 = x - 1.0 + 3.0*G3;
    vec4 w, d;
    w.x = dot(x, x);
    w.y = dot(x1, x1);
    w.z = dot(x2, x2);
    w.w = dot(x3, x3);
    w = max(0.6 - w, 0.0);
    d.x = dot(random3(s), x);
    d.y = dot(random3(s + i1), x1);
    d.z = dot(random3(s + i2), x2);
    d.w = dot(random3(s + 1.0), x3);
    w *= w;
    w *= w;
    d *= w;
    return dot(d, vec4(52.0));
}

float fbm(vec3 p) {
    float f = 0.0;
    float amp = 0.5;
    for(int i=0; i<5; i++) { // Seulement 5 octaves pour perf
        f += amp * snoise(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return f * 0.5 + 0.5;
}

// --- RAYMARCHING HELPERS ---
vec2 intersectSphere(vec3 ro, vec3 rd, vec3 center, float radius) {
    vec3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) return vec2(-1.0); // Pas d'intersection
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

// Map 3D pos to UV sphere based on direction from center (Triplanar-ish sphérique)
vec2 getUV(vec3 p) {
    vec3 dir = normalize(p - planetCenter);
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * 3.14159);
    float v = 0.5 - asin(dir.y) / 3.14159;
    return vec2(u, v);
}

// Density function
float sampleDensity(vec3 p) {
    float dist = length(p - planetCenter);
    float minR = planetRadius + cloudMinHeight;
    float maxR = planetRadius + cloudMaxHeight;
    
    // 1. Hauteur : Fade en bas et en haut
    float h = (dist - minR) / (maxR - minR);
    if(h < 0.0 || h > 1.0) return 0.0;
    
    // 2. Coverage (Carte 2D)
    // On fait tourner la texture en SYNC avec la planete
    vec2 uv = getUV(p);
    uv.x += cloudRotationOffset; 
    
    // Pour eviter le tiling visible ou le saut, on prend fract (UV wrap)
    // Texture sampler gere le wrap, mais c'est bien de garder uv normalise si besoin
    
    float coverage = texture(cloudCoverageMap, uv).w; // Le generateur met la densite dans Alpha (w)
    // Fallback si alpha est 1 partout (rgb)
    if(coverage > 0.99) coverage = texture(cloudCoverageMap, uv).r;

    if (coverage < 0.1) return 0.0; // Optimisation
    
    // 3. Forme 3D (Noise)
    // Scale du noise relative au rayon
    // On ralentit énormément l'animation du bruit ("le vent local")
    // C'était time * 0.05 -> trop rapide (effet "bouillant")
    // On passe a time * 0.002 -> tres lent, évolution naturelle
    vec3 noisePos = p * 0.005 + vec3(time * 0.002, 0.0, 0.0); 
    float shape = fbm(noisePos);
    
    // Erosion du nuage par le noise
    float baseCloud = coverage;
    // On creuse le nuage avec le noise
    float finalDensity = baseCloud * shape; 
    
    // Threshold pour avoir des trous
    finalDensity = smoothstep(0.3, 0.8, finalDensity);

    // Soft edges height
    finalDensity *= smoothstep(0.0, 0.2, h) * smoothstep(1.0, 0.8, h);
    
    return clamp(finalDensity * 2.0, 0.0, 1.0); // Density Multiplier
}

void main() {
    vec3 rayDir = normalize(FragPos - viewPos);
    vec3 ro = viewPos;
    
    // Limites de la couche nuageuse
    float minR = planetRadius + cloudMinHeight;
    float maxR = planetRadius + cloudMaxHeight;
    
    // --- INTERSECTION ET MARCHING OPTIMISE ---
    
    // 1. Intersection Sphere Externe (Le toit des nuages)
    // On veut savoir où le rayon SORT du volume nuageux global.
    vec2 hitOuter = intersectSphere(ro, rayDir, planetCenter, maxR);
    float tDist = hitOuter.y; // Le point de sortie (loin)
    
    // Si la sphere est derrière ou ratée
    if (tDist < 0.0) discard;
    
    // 2. Intersection Sphere Interne (Le sol des nuages)
    // On veut savoir si le rayon percute le "sol" nuageux avant de sortir.
    vec2 hitInner = intersectSphere(ro, rayDir, planetCenter, minR);
    
    // Si on touche le sol nuageux en face de nous (dist > 0)
    // On arrête le rayon à ce point d'impact.
    // On ignore ce qui se passe "derriere" la terre.
    if (hitInner.x > 0.0) {
        tDist = min(tDist, hitInner.x);
    }
    
    float tStart = max(0.0, hitOuter.x); // Si on est dehors, on commence au bord. Si dedans, à 0.
    float tEnd = tDist;
    
    if (tEnd <= tStart) discard;

    // Raymarching
    int steps = 64; 
    float marchingDist = tEnd - tStart;
    
    // Step Size adaptatif mais borné pour éviter les sauts géants quand on regarde l'horizon
    // Si on regarde l'horizon, marchingDist est grand.
    // Mais on ne veut pas rater les nuages locaux.
    // On limite le step size max.
    float stepSize = marchingDist / float(steps);
    
    vec3 p = ro + rayDir * tStart;
    float accumDensity = 0.0;
    vec3 cloudColor = vec3(0.0);
    vec3 lightDir = normalize(lightPos - planetCenter); 
    
    // Dithering
    float rnd = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898,78.233))) * 43758.5453);
    p += rayDir * stepSize * rnd;

    for(int i = 0; i < steps; i++) {
        if(accumDensity >= 1.0) break;
        
        // Safety: sortir si on dépasse tEnd
        // (Calcul distance approché)
        // if(distance(p, ro) > tEnd) break; 
        
        // Sampling
        // On check isInside radius bounds dans sampleDensity direct
        float dens = sampleDensity(p);
        
        if (dens > 0.01) {
             // Basic Lighting : Dot product avec le soleil
             float diffuse = clamp(dot(normalize(p - planetCenter), lightDir), 0.0, 1.0);
             // + Ambient
             diffuse = diffuse * 0.7 + 0.3;

             vec3 baseCol = vec3(1.0, 1.0, 1.0) * diffuse;
             
             // Absorption simple
             float alpha = dens * 0.6; // Opacite arbitraire
             
             cloudColor += baseCol * alpha * (1.0 - accumDensity);
             accumDensity += alpha;
        }
        
        p += rayDir * stepSize;
    }

    if (accumDensity <= 0.01) discard;
    
    // Fog atmospherique sur les nuages lointains ? (optionnel)
    
    FragColor = vec4(cloudColor, accumDensity);
}
