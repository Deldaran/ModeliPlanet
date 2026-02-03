#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aAltitude;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out float vAltitude;
out vec2 vTexCoords;
out vec3 vNormal;
out vec3 vFragPos;
out vec4 vFragPosLightSpace; // Transmission vers le Fragment Shader

void main() {
    vAltitude = aAltitude;
    vTexCoords = aTexCoords;
    vFragPos = vec3(model * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    
    // Position du point pour le calcul d'ombre
    vFragPosLightSpace = lightSpaceMatrix * vec4(vFragPos, 1.0);
    
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}