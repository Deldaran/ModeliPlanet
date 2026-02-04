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

// Fonction Remap avec Clamp intégré (Essentiel pour la sculpture)
float remap(float v, float minOld, float maxOld, float minNew, float maxNew) {
    return minNew + (v - minOld) * (maxNew - minNew) / (maxOld - minOld);
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

// --- SYSTEME METEO DYNAMIQUE (Schneider-like) ---
float getDensityForCloud(vec3 p, float weather, float hFrac) {
    // 1. Definition du Type de Nuage selon la densité météo
    // Weather < 0.5 : Stratus (Plat, fin)
    // Weather > 0.5 : Cumulus (Epais, grumeleux)
    float stratusHeight = smoothstep(0.0, 0.1, hFrac) * smoothstep(0.3, 0.2, hFrac);
    float cumulusHeight = smoothstep(0.1, 0.2, hFrac) * smoothstep(0.9, 0.6, hFrac);
    return mix(stratusHeight, cumulusHeight, smoothstep(0.4, 0.7, weather));
}

// Fonction de sculpture principale - MODE FINAL SYSTEMIQUE
float sampleCloudDensity(vec3 p) {
    float len = length(p - planetCenter);
    float hFrac = (len - (planetRadius + cloudMinHeight)) / (cloudMaxHeight - cloudMinHeight);
    if(hFrac < 0.0 || hFrac > 1.0) return 0.0;

    // --- A. WEATHER MAP (La "carte des pressions") ---
    // Tres basse frequence, bouge lentement. Pente douce.
    vec3 weatherPos = p * 0.00004 + vec3(time * 0.000005, 0.0, 0.0); 
    float bigWeather = texture(noiseTexture3D, weatherPos).r;
    
    // Echelle 2 : Moyenne fréquence (Pour créer des "vaguelettes" et briser les blocs)
    vec3 breakupPos = p * 0.00015 + vec3(time * 0.00001, 0.0, 0.0);
    float breakupNoise = texture(noiseTexture3D, breakupPos).g; // Canal Vert
    
    // On sculpte la météo : Grand nuage - (Moyen Bruit * 0.3)
    float compositeWeather = bigWeather - (breakupNoise * 0.30); // Reduit l'erosion (etait 0.35)
    
    // RETOUR A LA DISTRIBUTION UNIFORME (Sans bandes)
    // Seuil de 0.25 : Pas trop vide, pas trop plein.
    float weatherCoverage = smoothstep(0.25, 0.85, compositeWeather);
    
    // Boost de couverture pour faire "gonfler" les nuages existants
    weatherCoverage = clamp(weatherCoverage * 1.35, 0.0, 1.0);
    
    if (weatherCoverage < 0.01) return 0.0;

    // --- B. MODELISATION DE FORME (Basic Shape) ---
    // Bruit de forme moyenne fréquence
    vec3 shapePos = p * 0.0002 + vec3(time * 0.000015, time * -0.000005, 0.0);
    vec4 shapeSample = texture(noiseTexture3D, shapePos);
    float shapeFBM = shapeSample.g * 0.625 + shapeSample.b * 0.25 + shapeSample.a * 0.125;

    // L'EQUATION DE CROISSANCE :
    // Density = Remap(Noise, 1 - Coverage, 1, 0, 1)
    // Plus "weatherCoverage" est grand (zone de pluie/densité), plus le nuage grossit et fusionne.
    float baseCloud = remap(shapeFBM, 1.0 - weatherCoverage, 1.0, 0.0, 1.0);
    
    if (baseCloud <= 0.0) return 0.0;
    
    // --- C. APPLICATION DU TYPE (Stratus vs Cumulus) ---
    float densityHeightParams = getDensityForCloud(p, weatherCoverage, hFrac);
    baseCloud *= densityHeightParams;

    // --- D. EROSION FINE (Le "Vent") ---
    vec3 detailPos = p * 0.002 + vec3(time * 0.00005, 0.0, 0.0); 
    vec3 detailNoise = texture(noiseTexture3D, detailPos).gba;
    float highFreq = detailNoise.r * 0.625 + detailNoise.g * 0.25 + detailNoise.b * 0.125;
    
    float erosionModifier = mix(highFreq, 1.0 - highFreq, clamp(hFrac, 0.0, 1.0));
    float finalCloud = remap(baseCloud, erosionModifier * 0.25, 1.0, 0.0, 1.0);
    
    return clamp(finalCloud * 6.0, 0.0, 1.0);
}

// --- SCATTERING PHASE FUNCTION (Henyey-Greenstein) ---
// Dual lobe for realistic forward AND backward scattering
float henyeyGreenstein(float g1, float g2, float mixFactor, float cosTheta) {
    float g1_2 = g1 * g1;
    float g2_2 = g2 * g2;
    
    // Forward (Silver Lining)
    float forward = (1.0 - g1_2) / pow(1.0 + g1_2 - 2.0 * g1 * cosTheta, 1.5);
    
    // Backward (Glory/Rainbow side - brighter from space)
    float backward = (1.0 - g2_2) / pow(1.0 + g2_2 - 2.0 * g2 * cosTheta, 1.5);
    
    return mix(forward, backward, mixFactor) / 4.0 * 3.14159;
}

// --- ECLAIRAGE AVANCÉ (Beer-Powder + Multiple Scattering Approx) ---
float getLight(vec3 p, vec3 lightDir) {
    // 1. Planet Occlusion
    vec2 hitEarth = intersectSphere(p, lightDir, planetCenter, planetRadius - 10.0);
    if (hitEarth.x > 0.0) return 0.0; // Nuit

    // 2. Raymarch vers le Soleil (Qualité augmentée: 6 steps progessifs)
    float totalDens = 0.0;
    vec3 shadowP = p;
    float stepL = 20.0; // Pas initial petit pour capturer les détails de self-shadowing
    
    for (int i=0; i<6; i++) {
        shadowP += lightDir * stepL;
        // On n'a pas besoin de detailNoise pour l'ombre (trop lourd), juste la Density de base
        totalDens += sampleCloudDensity(shadowP) * stepL; 
        stepL *= 1.5; // On allonge le pas (LOD) pour aller chercher les nuages lointains
    }
    
    // --- BEER'S LAW (Ombre Classique) ---
    // exp(-density) -> Sombre très vide.
    float beer = exp(-totalDens * 0.05); // Densité optique ajustée
    
    // --- POWDER EFFECT (Le Secret du Réalisme) ---
    // 1 - exp(-density * 2)
    // Cela inverse la courbe : Les zones très denses deviennent un peu plus brillantes en surface
    // (Comme un tas de sucre : les grains renvoient la lumière).
    float powder = 1.0 - exp(-totalDens * 0.1);
    
    // Combine : Beer (Ombre foncée au coeur) * Powder (Bords brillants)
    return mix(beer, beer * powder + 0.2 * beer, 0.5); 
}

void main() {
    vec3 rayDir = normalize(FragPos - viewPos);
    vec3 ro = viewPos;
    
    // Cloud Layer
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
    
    // OCCLUSION PLANETAIRE
    vec2 hitSolid = intersectSphere(ro, rayDir, planetCenter, planetRadius);
    if (hitSolid.x > 0.0 && hitSolid.x < tEnd) {
         tEnd = min(tEnd, hitSolid.x);
    }
    
    // CAMERA POSITION LOGIC
    float camDist = length(ro - planetCenter);
    if (camDist > minR && camDist < maxR) {
        tStart = 0.0; 
    }
    else if (camDist <= minR) {
         tStart = hitIn.y;
         tEnd = hitOut.y;
    }
    
    if (tEnd <= tStart) discard;
    
    // Limite distance pour perfs
    if ((tEnd - tStart) > 80000.0) tEnd = tStart + 80000.0;

    // Raymarch Setup
    int steps = STEPS;
    float stepSize = (tEnd - tStart) / float(steps);
    vec3 p = ro + rayDir * tStart;
    
    vec3 lightDir = normalize(lightPos - planetCenter);
    float cosTheta = dot(rayDir, lightDir);
    
    // Phase Function (Comment la lumière traverse la gouttelette)
    // Forte en avant (Silver Lining) + Retro-reflexion (Glory)
    float phase = henyeyGreenstein(0.6, -0.3, 0.7, cosTheta);
    
    p += rayDir * stepSize * fract(sin(dot(gl_FragCoord.xy, vec2(12.9898,78.233))) * 43758.5453);

    vec3 sum = vec3(0.0);
    float transmittance = 1.0;
    
    // COULEURS AMBIANTES (Sky colors)
    vec3 sunColor = vec3(1.0, 0.95, 0.85) * 1.5; // Soleil puissant
    vec3 ambientHigh = vec3(0.3, 0.4, 0.6); // Bleu du ciel (Haut)
    vec3 ambientLow  = vec3(0.6, 0.55, 0.5); // Gris reflet sol (Bas)

    for(int i=0; i<steps; i++) {
        if(transmittance < 0.01) break;
        
        // Sampling
        float density = sampleCloudDensity(p);
        
        if(density > 0.001) {
            // Altitude locale (0=bas, 1=haut) pour l'ambiance
            float h = (length(p - planetCenter) - minR) / (maxR - minR);
            
            // Calcul Lumière Directe (Soleil + Ombre portée)
            float lightTrans = getLight(p, lightDir);
            
            // Calcul Lumière Ambiante (Gradient Vertical) + AMORTISSEMENT NUIT
            // Le haut du nuage reçoit le bleu du ciel, le bas reçoit le rebond du sol
            // Et on éclaire plus le bas si la densité est faible (lumière diffuse)
            vec3 ambient = mix(ambientLow, ambientHigh, h);
            
            // Fix Nuit: On éteint l'ambiance si on est du côté nuit de la planète
            // Dot: 1.0 = Midi, 0.0 = Terminateur, -1.0 = Minuit
            float sunDot = dot(normalize(p - planetCenter), lightDir);
            float dayFactor = smoothstep(-0.2, 0.1, sunDot); // Transition douce crépuscule
            ambient *= dayFactor;

            // SCATTERING ENERGY
            // Light = (Soleil * Ombre * Phase) + Ambiance
            vec3 scattering = sunColor * lightTrans * phase + ambient * 0.4;
            
            // Accumulation
            // alpha = 1 - exp(-densité * pas)
            float alpha = 1.0 - exp(-density * stepSize * 0.1); // 0.1 = densité optique
            
            sum += scattering * alpha * transmittance;
            transmittance *= (1.0 - alpha);
        }
        
        p += rayDir * stepSize;
    }

    if(transmittance >= 0.99) discard;

    // Output
    FragColor = vec4(sum, 1.0 - transmittance);
}
