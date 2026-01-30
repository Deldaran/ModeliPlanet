#version 330 core
out vec4 FragColor;

in vec3 vNormal;
in vec3 vFragPos;

uniform vec3 viewPos;
uniform vec3 lightPos;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDir = normalize(viewPos - vFragPos);
    vec3 lightDir = normalize(lightPos - vFragPos);

    // 1. Effet de Fresnel : l'opacité augmente sur les bords
    float intensity = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0);
    
    float lightFactor = smoothstep(-0.2, 0.5, dot(normal, lightDir));

    // 2. Éclairage : l'atmosphère n'est visible que face au soleil
    float atmosphereFacingSun = max(dot(normal, lightDir), 0.0);

    vec3 atmoColor = vec3(0.3, 0.6, 1.0); // Bleu ciel
    
    // On combine l'intensité du bord et la lumière du soleil
    float alpha = intensity * lightFactor * 0.8;

    FragColor = vec4(atmoColor, alpha);
}