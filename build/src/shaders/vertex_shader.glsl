#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aAltitude; // Reçu depuis Planet.hpp

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out float vAltitude; // On l'envoie au fragment shader

void main() {
    vAltitude = aAltitude;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}