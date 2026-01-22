#version 330 core
out vec4 FragColor;

in float vAltitude;
in vec3 vNormal;
in vec3 vFragPos;
in vec4 vFragPosLightSpace; // Position projetée du point de vue du soleil

uniform vec3 lightPos;
uniform sampler2D shadowMap;

float ShadowCalculation(vec4 fragPosLightSpace) {
    // 1. Transformer les coordonnées de [-1,1] vers [0,1]
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // 2. Récupérer la profondeur enregistrée dans la map
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    float currentDepth = projCoords.z;

    // 3. Shadow Acne Fix (le "Bias")
    // On ajuste le biais selon l'angle de la montagne
    float bias = max(0.05 * (1.0 - dot(vNormal, normalize(lightPos - vFragPos))), 0.005);
    
    // 4. Test d'ombre
    float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    
    // Si on est en dehors de la map du soleil, pas d'ombre
    if(projCoords.z > 1.0) shadow = 0.0;

    return shadow;
}
void main() {
    // 1. On force une couleur de base selon l'altitude
    vec3 color;
    if (vAltitude < 1.0) color = vec3(0.0, 0.3, 0.8);      // Bleu
    else if (vAltitude < 1.05) color = vec3(0.1, 0.8, 0.1); // Vert
    else color = vec3(0.7, 0.7, 0.7);                       // Gris

    // 2. On utilise une lumière simple (sans ombre pour l'instant)
    vec3 norm = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vFragPos);
    float diff = max(dot(norm, lightDir), 0.2); // 0.2 d'ambiance minimum

    FragColor = vec4(color * diff, 1.0);
}