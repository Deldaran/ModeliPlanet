#version 330 core
out vec4 FragColor;

in vec3 vPosition;
in vec3 vNormal;

uniform vec3 viewPos;
uniform vec3 lightPos; // Soleil
uniform vec3 planetCenter; // Centre planete
uniform float innerRadius; // Rayon Planete
uniform float outerRadius; // Rayon Atmosphere

// --- PARAMETRES PHYSIQUES (Rayleigh / Mie) ---
const float PI = 3.14159265359;
// Coefficients de dispersion (Bleu pour Rayleigh, Blanc pour Mie)
const vec3 betaRayleigh = vec3(3.8e-6, 13.5e-6, 33.1e-6); // Diffuse plus le bleu
const vec3 betaMie = vec3(21e-6); // Brume blanche
const float g = 0.76; // Anisotropie de Mie (Direction brume solaire)

// Intersection Rayon/Sphere
vec2 raySphereIntersect(vec3 r0, vec3 rd, vec3 s0, float sr) {
    float a = dot(rd, rd);
    vec3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sr * sr);
    float delta = b * b - 4.0 * a * c;
    if (delta < 0.0) {
        return vec2(-1.0, -1.0);
    }
    return vec2(
        (-b - sqrt(delta)) / (2.0 * a),
        (-b + sqrt(delta)) / (2.0 * a)
    );
}

// Fonction de Phase
float rayleighPhase(float mu) {
    return 3.0 * (1.0 + mu * mu) / (16.0 * PI);
}
float miePhase(float mu) {
    return 3.0 / (8.0 * PI) * ((1.0 - g * g) * (1.0 + mu * mu)) / ((2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * mu, 1.5));
}

void main() {
    // 1. Vecteurs principaux
    vec3 rayStart = viewPos;
    vec3 rayDir = normalize(vPosition - viewPos);
    vec3 lightDir = normalize(lightPos - vPosition); // Direction vers le soleil
    float distToCam = length(vPosition - viewPos);

    // --- MODELE APPROXIMATIF PRO AMELIORE ---
    
    // 1. Orientation Camera vs Surface
    // Si on est dans l'atmosphere, on regarde depuis l'interieur.
    // vNormal pointe vers l'exterieur. 
    // Si dot(Nrm, ViewDir) < 0 : On regarde la planete de l'exterieur (Face).
    // Si dot(Nrm, ViewDir) > 0 : On regarde 'l'interieur' de la sphere atmosphere (Depuis le sol ou l'espace mais de l'interieur ?)
    // Attention: vNormal est la normale de la sphere atmosphere (centre -> point).
    // RayDir est Cam -> Point.
    // Dot(N, RayDir) est negatif si on regarde la sphere de dehors.
    
    vec3 N = normalize(vNormal);
    float orientation = dot(N, rayDir); // < 0 => Vue Exterieure (Face). > 0 => Impossible pour une sphere convexe vue de dehors, mais possible si cam dedans.
    
    // Distinguer Cam Inside / Outside via Uniform ou distance
    // float camDist = length(viewPos); // Supposant centre en 0,0,0 local
    // Mais vPosition est world space.
    // On va utiliser une approximation simple base sur l'effet visuel dsir.

    vec3 skyBlue = vec3(0.3, 0.5, 0.9); // Ciel un peu plus clair
    vec3 skyHorizon = vec3(0.6, 0.8, 1.0); // Horizon blanc/bleut
    vec3 sunsetColor = vec3(1.0, 0.4, 0.1);

    float alpha = 0.0;
    vec3 finalColor = vec3(0.0);
    float dayFactor = 0.0;

    float sunAngle = dot(N, lightDir);
    dayFactor = smoothstep(-0.25, 0.25, sunAngle); // Terminateur plus doux

    // --- LOGIQUE HYBRIDE ---
    
    // CAS 1: VUE DEPUIS L'ESPACE (Bords brillants bass sur Fresnel)
    // On approxime "Vue Espace" si on regarde une surface dont la normale est oppose au rayon.
    // Mais pour une atmo volume, c'est complexe.
    // On utilise Fresnel simple : bords de la sphere = plus d'atmo traverse.
    
    float mu = dot(N, -rayDir);
    float fresnel = pow(1.0 - max(mu, 0.0), 4.0);
    
    // CAS 2: DIFFUSION AU SOL (Zenith clair, Horizon dense)
    // Si on regarde vers l'horizon (mu proche de 0), c'est dense.
    // Si on regarde au zenith (mu proche de 1), c'est clair.
    
    // On mixe les deux approches.
    // Densit de base
    float atmosphereDensity = fresnel * 2.0; 
    
    // Si on est "dedans" (comment le savoir ?), on veut TOUJOURS du ciel sauf si c'est la nuit.
    // Si camera est proche de la surface (distToCam petit ?) non.
    
    // Hack visuel :
    // On veut que "derriere" la planete (nuit) ce soit transparent.
    // On veut que les bords soient bleus.
    // On veut que le "centre" (face planete) soit transparent pour voir le sol.
    
    // Si on regarde le sol (centre de l'ecran quand on est dans l'espace), fresnel est bas -> alpha bas -> on voit le sol. C'est bon.
    // Si on regarde l'horizon depuis l'espace, fresnel haut -> alpha haut -> halo bleu. C'est bon.
    
    // PROBLEME : Depuis le sol, quand on regarde le ciel (Zenith), on regarde "l'interieur" de la sphere. 
    // Normalement on renderait un "Skybox" pass ou un FullScreen Quad, mais ici on rend une Sphere Gante.
    // Donc si on est au sol, on est DANS la sphere.
    // Les faces sont "Back Faces" si on n'a pas cull face.
    // Si on a cull face front, on voit l'interieur.
    // La normale VNormal pointe toujours vers l'EXTERIEUR du centre.
    // Donc si on est dedant et qu'on regarde le ciel:
    // RayDir va vers le ciel. Normal va vers le ciel. Dot(N, RayDir) > 0.
    
    // normal va vers l'extérieur. Si on est dehors, on regarde une face frontale (dot < 0).
    // Si on est dedans, on voit les faces internes (dot > 0, backface).
    
    // --- DETECTION INSIDE/OUTSIDE ROBUSTE ---
    float distCamCenter = length(viewPos - planetCenter);
    
    // On est "physiquement" dans l'atmosphere si dist < Radius
    bool isInside = distCamCenter < (outerRadius - 5.0); 

    // Altitude relative pour la densité
    float altitude = distCamCenter - innerRadius;
    float thickness = outerRadius - innerRadius;
    float h = clamp(altitude / thickness, 0.0, 1.0);
    
    // Densité atmospherique globale
    float globalDensity = max(0.4, exp(-h * 3.0)); 

    if (isInside) {
        // --- VUE INTERIEURE ---
        if (orientation > 0.0) {
             // Regarde le ciel (Zenith)
             // Back Face visible
             float viewAngle = orientation; 
             
             vec3 zenithColor = vec3(0.0, 0.3, 0.8); 
             vec3 horizonColor = vec3(0.5, 0.7, 0.9);
             
             finalColor = mix(horizonColor, zenithColor, pow(viewAngle, 0.4));
             alpha = (0.3 + 0.5 * (1.0 - viewAngle)) * globalDensity;
        } else {
             // Regarde le sol (Horizon/Bas)
             // Front Face visible (hemisphere opposé)
             finalColor = vec3(0.5, 0.7, 0.9);
             alpha = 0.3 * globalDensity;
        }

    } else {
        // --- VUE EXTERIEURE (Espace) ---
        // Back Faces (Arrière de l'atmo) doivent être invisibles
        if (orientation > 0.0) discard;
        
        // Front Faces (Bords brillants)
        float mu = dot(N, -rayDir); // 0..1
        float fresnel = pow(1.0 - max(mu, 0.5), 3.0); // Clamp mu pour eviter l'artefact central
        
        finalColor = skyBlue;
        alpha = fresnel * 1.5;
    }

    // --- SOLEIL ET MIE SCATTERING ---
    // Halo autour du soleil
    float sunViewAngle = dot(rayDir, lightDir);
    float sunHalo = pow(max(sunViewAngle, 0.0), 32.0);
    
    finalColor += vec3(1.0, 0.9, 0.8) * sunHalo * 0.5;

    // --- COUCHER DE SOLEIL ---
    // Si on est proche du terminateur
    if (sunAngle > -0.2 && sunAngle < 0.2) {
        float sunsetFactor = 1.0 - abs(sunAngle) * 5.0;
        finalColor = mix(finalColor, sunsetColor, sunsetFactor * 0.8);
    }
    
    // --- FINAL MIX ---
    finalColor *= dayFactor;
    alpha *= dayFactor;
    
    // Clamp
    alpha = clamp(alpha, 0.0, 1.0);
    
    FragColor = vec4(finalColor, alpha);
}
