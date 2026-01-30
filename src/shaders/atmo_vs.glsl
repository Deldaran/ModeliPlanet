#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 3) in vec3 aNormal; // Vérifiez l'index de vos normales

out vec3 vNormal;
out vec3 vFragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vFragPos = vec3(model * vec4(aPos, 1.0));
    vNormal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}