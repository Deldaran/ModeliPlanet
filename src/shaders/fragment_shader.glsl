#version 330 core
out vec4 FragColor;

in float vAltitude;
in vec2 vTexCoords;
in vec3 vNormal;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform sampler2D shadowMap;
uniform sampler2D planetData;
uniform sampler2D cloudTexture; 
uniform sampler3D noiseTexture3D;
uniform float cloudRotationOffset;
uniform vec3 planetCenter;
uniform float time; // Needed for noise animation

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}
float pNoise(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    float a = rand(i);
    float b = rand(i + vec2(1.0, 0.0));
    float c = rand(i + vec2(0.0, 1.0));
    float d = rand(i + vec2(1.0, 1.0));
    vec2 u = f*f*(3.0-2.0*f);
    return mix(a, b, u.x) + (c - a)* u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float ShadowCalculation(vec4 fragPosLightSpace) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(vNormal, normalize(lightPos - vFragPos))), 0.0005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    if(projCoords.z > 1.0) shadow = 0.0;
    return shadow;
}

void main() {
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vFragPos);
    vec3 viewDir = normalize(viewPos - vFragPos);
    float shadow = ShadowCalculation(vFragPosLightSpace);
    
    // --- OMBRES NUAGES (CAST SHADOWS) ---
    float cloudAltitude = 120.0;
    
    // Direction vers le soleil
    vec3 L = normalize(lightPos - planetCenter);
    vec3 planetN = normalize(vFragPos - planetCenter);
    
    // Parallax simplifiee et stabilisee
    // On limite l'etirement de l'ombre en clampant le dot product à 0.25
    // Cela evite que l'ombre ne couvre toute la planete lors des angles rasants (Bug du noir complet)
    float NdotL = max(dot(planetN, L), 0.25); 
    float distToCloud = cloudAltitude / NdotL;
    
    // Position projetee dans la couche nuageuse
    vec3 cloudPos = vFragPos + L * distToCloud;
    
    // UV Mapping
    vec3 pRel = cloudPos - planetCenter;
    vec3 dir = normalize(pRel);
    vec2 cloudUV = vec2(
        0.5 + atan(dir.z, dir.x) / (2.0 * 3.14159265),
        0.5 - asin(dir.y) / 3.14159265
    );
    cloudUV.x += cloudRotationOffset; // Sync avec la rotation des nuages
    
    // Sampling & Evolution "Naturelle" (Copie exacte de f_cloud.glsl)
    // 1. Lire la "Carte" (Forme générale des continents nuageux)
    vec4 cSample = texture(cloudTexture, cloudUV);
    float mapCoverage = cSample.r * cSample.a; // Support PNG transparent
    
    // 2. Lire le "Vent/Chaos" (Bruit 3D qui fait bouger et evoluer les formes)
    // On utilise les memes coordonnees que le shader de nuages (Vent ralenti x10)
    vec3 animPos = pRel * 0.0003 + vec3(time * 0.0005, 0.0, 0.0);
    float lowFreq = texture(noiseTexture3D, animPos).r;
    
    float coverage = mapCoverage * mapCoverage; // Simple quadratic falloff
    
    float combinedFn = 0.0;
    // Si pas de nuage sur la carte, on sort direct (Optimisation)
    if (coverage > 0.1) {
        // Meme echelle que le nouveau shader de nuages
        vec3 shapePos = pRel * 0.0003 + vec3(time * 0.00005, 0.0, 0.0);
        float baseNoise = texture(noiseTexture3D, shapePos).r;
        
        // Formule Nubis simplifié pour l'ombre
        // On seuille le bruit par l'inverse de la coverage
        float baseCloud = clamp((baseNoise - (1.0 - coverage)) / coverage, 0.0, 1.0);
        
        combinedFn = baseCloud;
    }

    // Calcul final de l'ombre
    float cloudShadow = smoothstep(0.1, 0.6, combinedFn) * 0.8;
    
    // Fade Horizon: Reduit l'ombre sur les bords de la planete pour eviter les artefacts
    cloudShadow *= smoothstep(0.0, 0.3, dot(planetN, L)); // Utilise le vrai dot ici
    
    shadow = max(shadow, cloudShadow);
    
    float diff = max(dot(norm, lightDir), 0.0);

    float heightHD = texture(planetData, vTexCoords).r;
    float grain = pNoise(vTexCoords * 4000.0); 
    float a_detailed = heightHD + (grain * 0.02 - 0.01);

    vec3 deepOcean  = vec3(0.005, 0.01, 0.25); 
    vec3 shallowSea = vec3(0.0, 0.4, 0.6);    
    vec3 beach      = vec3(0.76, 0.70, 0.50);    
    vec3 forest     = vec3(0.05, 0.25, 0.05);   
    vec3 plain      = vec3(0.2, 0.4, 0.15); 
    vec3 rock       = vec3(0.35, 0.32, 0.30);  
    vec3 snow       = vec3(0.95, 0.95, 1.0);  

    vec3 color;
    float specular = 0.0;

    if (a_detailed < 0.5) {
        float t = a_detailed / 0.5; 
        color = mix(deepOcean, shallowSea, clamp(t * t, 0.0, 1.0));
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
        color += vec3(0.7) * spec * (1.0 - shadow);
    } 
    else {
        float h = (a_detailed - 0.5) * 2.0; 
        if (h < 0.03) {
            color = beach * (0.9 + 0.2 * grain); 
        } 
        else if (h < 0.35) {
            float t = smoothstep(0.03, 0.35, h);
            vec3 vegetation = mix(plain, forest, grain * 0.5 + 0.5); 
            color = mix(beach, vegetation, t);
        } 
        else if (h < 0.65) {
            float t = smoothstep(0.35, 0.65, h);
            vec3 rockDetail = rock * (0.8 + 0.4 * grain); 
            color = mix(forest, rockDetail, t);
        } 
        else {
            float t = smoothstep(0.65, 0.85, h);
            color = mix(rock, snow, t);
        }
    }

    vec3 ambient = 0.05 * color;
    vec3 lighting = (ambient + (1.0 - shadow) * (diff * color + specular));
    FragColor = vec4(lighting, 1.0);
}
