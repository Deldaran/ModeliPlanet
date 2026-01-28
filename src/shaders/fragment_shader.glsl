#version 330 core
out vec4 FragColor;

in float vAltitude;
in vec3 vNormal;
in vec3 vFragPos;
in vec4 vFragPosLightSpace;

uniform vec3 lightPos;
uniform vec3 viewPos; // Ajoute l'uniform de la position caméra pour le reflet
uniform sampler2D shadowMap;

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
    // 1. VARIABLES DE BASE (Calculées au début pour être dispo partout)
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vFragPos);
    vec3 viewDir = normalize(viewPos - vFragPos);
    float shadow = ShadowCalculation(vFragPosLightSpace);
    float diff = max(dot(norm, lightDir), 0.0);

    // 2. PALETTE DE COULEURS
    vec3 deepOcean  = vec3(0.01, 0.05, 0.15); 
    vec3 shallowSea = vec3(0.0, 0.4, 0.6);    
    vec3 beach      = vec3(0.8, 0.7, 0.5);    
    vec3 forest     = vec3(0.1, 0.35, 0.1);   
    vec3 rock       = vec3(0.4, 0.38, 0.35);  
    vec3 snow       = vec3(0.95, 0.95, 1.0);  

    vec3 color;
    float specular = 0.0;
    float a = vAltitude;

    // 3. LOGIQUE DE DISTRIBUTION
    if (a <= 0.5) {
        // ZONE EAU (Bruit entre 0.0 et 0.5)
        float t = smoothstep(0.0, 0.5, a);
        color = mix(vec3(0.01, 0.05, 0.2), vec3(0.0, 0.4, 0.6), t);
        
        // Reflet soleil sur l'eau
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
        color += vec3(0.5) * spec * (1.0 - shadow);
    } 
    else {
        // ZONE TERRE (Bruit entre 0.5 et 1.0)
        // On recalcule un ratio h entre 0 et 1 pour la terre ferme
        float h = (a - 0.5) * 2.0; 

        if (h < 0.05) {
            color = vec3(0.8, 0.7, 0.5); // Plage
        } else if (h < 0.4) {
            float t = smoothstep(0.05, 0.4, h);
            color = mix(vec3(0.8, 0.7, 0.5), vec3(0.1, 0.4, 0.1), t); // Plaine
        } else if (h < 0.7) {
            float t = smoothstep(0.4, 0.7, h);
            color = mix(vec3(0.1, 0.4, 0.1), vec3(0.4, 0.35, 0.3), t); // Roche
        } else {
            float t = smoothstep(0.7, 1.0, h);
            color = mix(vec3(0.4, 0.35, 0.3), vec3(0.95, 0.95, 1.0), t); // Neige
        }
    }

    // 4. CALCUL FINAL
    vec3 ambient = 0.15 * color;
    // On ajoute le spéculaire uniquement s'il n'y a pas d'ombre
    vec3 result = (ambient + (1.0 - shadow) * (diff * color + specular));

    FragColor = vec4(result, 1.0);
}