#version 330 core
out vec4 FragColor;
in float vAltitude;

void main() {
    // 1. Océans (Altitude la plus basse)
    if (vAltitude < 1.0) {
        FragColor = vec4(0.0, 0.2, 0.5, 1.0); // Bleu foncé
    } 
    // 2. Plages / Bord de mer
    else if (vAltitude < 1.02) {
        FragColor = vec4(0.9, 0.8, 0.5, 1.0); // Sable
    }
    // 3. Plaines et forêts
    else if (vAltitude < 1.08) {
        FragColor = vec4(0.2, 0.5, 0.1, 1.0); // Vert
    }
    // 4. Montagnes hautes
    else if (vAltitude < 1.12) {
        FragColor = vec4(0.4, 0.3, 0.2, 1.0); // Brun
    }
    // 5. Neige (Sommets)
    else {
        FragColor = vec4(1.0, 1.0, 1.0, 1.0); // Blanc
    }
}