#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 3) in vec3 aNormal;

out vec3 vPosition;
out vec3 vNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    vPosition = vec3(model * vec4(aPos, 1.0));
    vNormal = normalize(mat3(transpose(inverse(model))) * aNormal);
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
