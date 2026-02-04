#define NOMINMAX
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "Planet.hpp"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model.hpp"
#include "stb_image.h" // Nécessaire pour charger la texture dans main

#include <windows.h>

// --- Variables Globales ---
Camera camera(glm::vec3(0.0f, 2.0f, 15.0f));
float lastX = 400, lastY = 300;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool showRay = false;
unsigned int lineVAO, lineVBO;

bool wireframeMode = false; 
bool pKeyPressed = false;

// Time Control
float timeScale = 1.0f;
double simulatedTime = 0.0; 
bool tKeyPressed = false;
bool gKeyPressed = false;
bool rKeyPressed = false;

// Paramètres de l'univers
// Unité du joueur (1.0f)
float playerUnit = 1.0f; // taille de l'entité (1f)
// Ne pas modifier les planètes automatiquement ici — garder leurs tailles d'origine
float TerreRadius = 1000.0f; // PLANETE GEANTE (X10 par rapport a 90)
float SoleilRadius = 3000.0f; // Soleil plus grand aussi
float planetDistance = 50000.0f; // Plus loin
// Ralentissement global pour effet "Realiste"
// 0.01 c'etait ~10min par orbite. On passe a 0.0005 (~3h par orbite)
float planetAngularSpeed = 0.0005f; 

glm::vec3 lastPlanetPos(0.0f);
float baseCameraSpeed = 5.0f; // Vitesse de caméra augmentée pour échelle géante
float playerHeight = 1.6f; // Hauteur du joueur corrigée pour ne pas traverser le sol
float planetSelfRotationSpeed = 0.005f; // Ralenti aussi la rotation sur elle meme
float groundRotationThreshold = 0.05f; // distance seuil pour considérer la caméra 'posée' sur la surface
// Third person / Ironman
bool thirdPerson = true;
// initial player position will be set relative to the sun radius (updated below after SoleilRadius is known)
glm::vec3 ironmanPos = glm::vec3(0.0f, 3.0f, 55.0f);
glm::vec3 ironmanForward = glm::vec3(0.0f, 0.0f, -1.0f);
float ironmanYaw = -90.0f;
float ironmanPitch = 0.0f;
float ironmanSpeed = 25.0f;
float cameraFollowDistance = 5.0f; // Plus loin pour l'orbite
float cameraFollowHeight = 0.0f; // Centrée
float cameraSmooth = 10.0f; 
float ironmanModelScale = 0.01f; 

// --- PHYSICS SYSTEM (KSP Style) ---
glm::vec3 ironmanVelocity(0.0f);
glm::vec3 ironmanForce(0.0f); // Accumulateur de forces
float ironmanMass = 1.0f;     // Masse arbitraire (kg)
float thrustPower = 25.0f;    // Puissance des propulseurs RCS/Jetpack

// Gravité : GM = g * R^2.
// Si on veut g ~ 9.81 sur Terre (R=1000), GM = 9.81 * 1000 * 1000 = 9,810,000
// Ajustons pour le "fun" du gameplay
float G_MASS_PRODUCT = 9.81f * 1000.0f * 1000.0f; 

// Camera Orbital State
float camOrbitYaw = 0.0f;
float camOrbitPitch = 20.0f;
float camOrbitDist = 5.0f;
bool cameraTargetPlanet = false; // F3 Mode: Orbit Planet vs Orbit Player
bool f3Pressed = false;          // Toggle latch

// --- Callbacks ---
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos; lastY = ypos;

    // En mode ThirdPerson, la souris controle l'ORBITE de la camera autour du joueur (Style KSP/MMO)
    // Et non la rotation du joueur
    if (thirdPerson) {
        // En maintenant clic droit pour bouger la cam ? 
        // Pour l'instant on fait Always Look
        float sensitivity = 0.2f;
        camOrbitYaw += xoffset * sensitivity;
        camOrbitPitch += yoffset * sensitivity;
        
        // Clamp Pitch
        if (camOrbitPitch > 89.0f) camOrbitPitch = 89.0f;
        if (camOrbitPitch < -89.0f) camOrbitPitch = -89.0f;

    } else {
        camera.ProcessMouseMovement(xoffset, yoffset);
    }
}

void processInput(GLFWwindow *window, glm::vec3 terrePos) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    // Time Scale Controls
    // T: Accelerate
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS && !tKeyPressed) {
        timeScale *= 2.0f;
        std::cout << "Time Scale: " << timeScale << "x" << std::endl;
        tKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_RELEASE) tKeyPressed = false;

    // G: Decelerate
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS && !gKeyPressed) {
        timeScale *= 0.5f;
        if(timeScale < 0.01f) timeScale = 0.01f;
        std::cout << "Time Scale: " << timeScale << "x" << std::endl;
        gKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_RELEASE) gKeyPressed = false;
    
    // R: Reset
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed) {
        timeScale = 1.0f;
        std::cout << "Time Scale: 1.0x" << std::endl;
        rKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE) rKeyPressed = false;

    if (thirdPerson) {
        // --- CONTROLES PHYSIQUES (RCS / JETPACK) ---
        // On n'affecte plus la position, mais on applique une FORCE
        
        glm::vec3 inputForce(0.0f);
        
        // Orientation relative à la caméra pour les contrôles (EVA Style)
        glm::vec3 camFront = camera.Front;
        glm::vec3 camRight = camera.Right;
        glm::vec3 camUp    = camera.Up;

        float currentThrust = thrustPower;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) currentThrust *= 2.0f;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputForce += camFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputForce -= camFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputForce -= camRight;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputForce += camRight;
        
        // Monter/Descendre (RCS Up/Down)
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) inputForce += camUp;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) inputForce -= camUp;

        // Normaliser si on appuie sur plusieurs touches pour pas aller plus vite en diagonale
        if (glm::length(inputForce) > 0.1f) {
            inputForce = glm::normalize(inputForce) * currentThrust;
            
            // Le joueur regarde dans la direction où il pousse (Optionnel)
            // ironmanForward = glm::normalize(inputForce);
        }

        // Ajouter cette force à l'accumulateur global
        ironmanForce += inputForce;
        
        // Stabilisateur d'inertie (SAS) : DESACTIVÉ pour ORBITE PURE
        // Si on veut une orbite realiste, on ne doit pas freiner quand on lache les touches
        /*
        float sasFactor = 1.0f; 
        if (glm::length(inputForce) < 0.1f) {
            ironmanForce -= ironmanVelocity * sasFactor;
        }
        */

     /* Code clean-up: ancienne logique supprimee */
    } else {
         float camSpeed = 50.0f * deltaTime;
         if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard("FORWARD", camSpeed);
         if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard("BACKWARD", camSpeed);
         if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard("LEFT", camSpeed);
         if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard("RIGHT", camSpeed);
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) { wireframeMode = !wireframeMode; pKeyPressed = true; }
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) {
        pKeyPressed = false;
    }

    if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        // Reset free camera position above the planet
        camera.Position = terrePos + glm::vec3(0.0f, TerreRadius + playerUnit * 2.0f, 5.0f);
        std::cout << "Camera reset to above the planet." << std::endl;
    }
    
    // Toggle Ray
    static bool lPressed = false;
    if(glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        if(!lPressed) {
            showRay = !showRay;
            std::cout << "Show Ray: " << (showRay ? "ON" : "OFF") << std::endl;
            lPressed = true;
        }
    } else {
        lPressed = false;
    }

    // Toggle F3: Camera Orbit Target (Player vs Planet)
    if(glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
        if (!f3Pressed) {
            cameraTargetPlanet = !cameraTargetPlanet;
            f3Pressed = true;
            std::cout << "Camera Mode: " << (cameraTargetPlanet ? "ORBIT PLANET" : "ORBIT PLAYER") << std::endl;
            
            if (cameraTargetPlanet) {
                // Swith to Planet: Set distance to see the whole planet
                camOrbitDist = TerreRadius * 4.0f; 
            } else {
                // Switch to Player: Reset distance close
                camOrbitDist = 5.0f;
            }
        }
    } else {
        f3Pressed = false;
    }
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = NULL;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    // Fenetre Classique (Avec barre de titre) mais Maximisée
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE); 
    
    // On laisse GLFW gérer les hints par défaut (DECORATED = TRUE)
    // NULL pour le moniteur = Mode fenêtré.
    window = glfwCreateWindow(1280, 720, "Planet Engine", NULL, NULL);
    
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    AllocConsole();
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); 
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- UTILS : Chargement Texture ---
    // Fonction lambda locale ou bloc pour charger la texture
    unsigned int planetTexture;
    glGenTextures(1, &planetTexture);
    glBindTexture(GL_TEXTURE_2D, planetTexture);
    // Paramètres
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Clamp en Y pour éviter les pôles bizarres
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    int width, height, nrChannels;
    unsigned char *data = stbi_load("planet_heightmap_hd.png", &width, &height, &nrChannels, 0);
    if (data) {
        // Note: La heightmap est en RGB, on peut l'utiliser telle quelle
        GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;
        if (nrChannels == 1) format = GL_RED;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Texture Planete chargee: " << width << "x" << height << std::endl;
        stbi_image_free(data);
    } else {
        std::cout << "ERREUR: Impossible de charger planet_heightmap_hd.png" << std::endl;
    }

    unsigned int cloudTexture;
    glGenTextures(1, &cloudTexture);
    glBindTexture(GL_TEXTURE_2D, cloudTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    unsigned char *cloudData = stbi_load("planet_clouds.png", &width, &height, &nrChannels, 0);
    if (cloudData) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, cloudData);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Texture Nuages chargee: " << width << "x" << height << std::endl;
        stbi_image_free(cloudData);
    } else {
        std::cout << "ERREUR: Impossible de charger planet_clouds.png" << std::endl;
    }

    // --- CHARGEMENT TEXTURE 3D (VOLUME NOISE) ---
    unsigned int volumeCloudTex;
    glGenTextures(1, &volumeCloudTex);
    glBindTexture(GL_TEXTURE_3D, volumeCloudTex);
    { // Scope pour les variables locales
        int size = 128;
        // RGBA = 4 channels
        std::vector<unsigned char> noiseData(size * size * size * 4);
        
        // Read file
        std::string noisePath = "assets/noise_shape_128.bin";
        std::ifstream noiseFile(noisePath, std::ios::binary);
        if(noiseFile) {
            noiseFile.read((char*)noiseData.data(), noiseData.size());
            noiseFile.close();
            
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, size, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, noiseData.data());
            
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            // MIRRORED_REPEAT prevents hard seams at axis 0
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_MIRRORED_REPEAT);
            
            std::cout << "Texture 3D (Noise) chargee avec succes." << std::endl;
        } else {
            std::cerr << "ERREUR: Impossible de charger " << noisePath << ". Avez-vous lance generate_noise3d ?" << std::endl;
        }
    }


    // --- Objets ---
    // On augmente la taille de la planète et le niveau de détail
    float displayTerreRadius = TerreRadius; // Utiliser la variable globale modifiée plus haut (on va la changer dans une autre edit) 
    Planet soleil(SoleilRadius), terre(TerreRadius), terreNuages(TerreRadius * 1.012f), terreAtmosphere(TerreRadius * 1.025f);
    
    soleil.generate(4); soleil.setupBuffers();
    
    // Génération Planète HD :
    // 1. Géométrie de base (Subdivision 8 = ~1.3M polys, assez lourd mais OK pour desktop)
    // Pour une "Très Grande" planète, 8 est un minimum pour éviter l'effet "Low Poly" à l'horizon.
    std::cout << "Generation de la geometrie planetaire..." << std::endl;
    terre.generate(8); 
    
    // 2. Chargement des données de relief (CPU) pour la physique
    if(terre.loadHeightMapFromImage("planet_heightmap_hd.png")) {
        // Appliquer le relief sur les sommets réels (Physique + Visuel géométrique)
        // Strength faible (0.05) car le rayon est grand (1000 * 0.05 = 50m de relief), ajuster selon besoin
        terre.applyRectangularNoise(0.05f); 
    } else {
        // Fallback
        terre.generateHeightMap();
        terre.applyRectangularNoise(0.15f);
    }
    terre.calculateNormals(); 
    terre.setupBuffers();
    
    terreAtmosphere.generate(7); terreAtmosphere.setupBuffers();
    
    std::cout << "Generation des Nuages..." << std::endl;
    // Subdivision egale a l'atmosphere pour etre lisse
    terreNuages.generate(7); terreNuages.setupBuffers();

    // --- Shaders ---
    Shader shaderSoleil("shaders/v_soleil.glsl", "shaders/f_soleil.glsl");
    Shader shaderPlanete("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
    Shader shaderShadow("shaders/v_shadow.glsl", "shaders/f_shadow.glsl");
    Shader shaderAtmosphere("shaders/atmo_vs.glsl", "shaders/atmo_fs.glsl");
    Shader shaderNuages("shaders/v_cloud.glsl", "shaders/f_cloud.glsl");
    shaderNuages.use();
    shaderNuages.setInt("cloudTexture", 0);

    Shader shaderModel("shaders/v_model.glsl", "shaders/f_model.glsl");
    shaderModel.use();
    shaderModel.setInt("texture_diffuse1", 0);

    // Load real Ironman model from assets
    Model ironmanModel("assets/ironman.FBX");

    // Shadow Map Config
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::cout << "--- ENGINE START ---" << std::endl;
    glGenVertexArrays(1, &lineVAO);
    glGenBuffers(1, &lineVBO);
    Shader shaderLine("shaders/v_line.glsl", "shaders/f_line.glsl");

    glm::vec3 sunPos(0.0f); // Position du soleil a l'origine

    // Spawn Player near Planet Surface (at time = 0)
    // Planet start pos at t=0 is (0, 0, planetDistance)
    glm::vec3 startPlanetPos = glm::vec3(0.0f, 0.0f, planetDistance);
    
    // POSITION DE DEPART: EN ORBITE (HAUTE ALTITUDE)
    // Au lieu de 5.0f (Sol), on met 300.0f (Espace/Haute Atmosphère)
    ironmanPos = startPlanetPos + glm::vec3(0.0f, TerreRadius + 300.0f, 0.0f);
    
    // VITESSE ORBITALE INITIALE
    // Pour éviter de tomber comme une pierre, on donne une vitesse tangentielle
    // V = sqrt(GM / R) approx 87.0f pour R=1300
    ironmanVelocity = glm::vec3(87.0f, 0.0f, 0.0f);
    
    lastPlanetPos = startPlanetPos;
    
    // Reset camera too
    camera.Position = ironmanPos + glm::vec3(0.0f, 5.0f, 10.0f);
    camera.Front = glm::normalize(ironmanPos - camera.Position);

    // If third-person, position the camera initially based on ironman
    if (thirdPerson) {
        glm::vec3 desiredCamInit = ironmanPos - ironmanForward * cameraFollowDistance + glm::vec3(0.0f, cameraFollowHeight, 0.0f);
        camera.Position = desiredCamInit;
        camera.Front = glm::normalize(ironmanPos - camera.Position);
        camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
        camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
        // initial smoothing (lower to avoid visual tremble)
        cameraSmooth = 10.0f;
    }

    while (!glfwWindowShouldClose(window)) {
        float currentFrameReal = glfwGetTime();
        deltaTime = currentFrameReal - lastFrame;
        lastFrame = currentFrameReal;
        
        // Time Scale Logic
        simulatedTime += deltaTime * timeScale;
        float currentFrame = (float)simulatedTime;
        float dtSim = deltaTime * timeScale;

        glm::vec3 terrePos = glm::vec3(
            sin(currentFrame * planetAngularSpeed) * planetDistance, 
            0.0f, 
            cos(currentFrame * planetAngularSpeed) * planetDistance
        );

        processInput(window, terrePos);

        // --- SYSTEME PHYSIQUE (Newtonian / KSP) ---
        
        // 1. Déplacer le joueur avec la planète (Frame of Reference Relatif)
        ironmanPos += (terrePos - lastPlanetPos);

        // 2. Calcul Physique Vectorielle
        glm::vec3 dirToPlanet = terrePos - ironmanPos;
        float distToPlanet = glm::length(dirToPlanet);
        glm::vec3 gravityDir = glm::normalize(dirToPlanet); // Pointe vers le centre
        
        if (distToPlanet < 1.0f) distToPlanet = 1.0f;

        // F_gravity = G * M * m / r^2 => a = GM / r^2
        float gravityAccel = G_MASS_PRODUCT / (distToPlanet * distToPlanet);
        
        // --- RESISTANCE DE L'AIR (DRAG) ---
        // Altitude approximative par rapport au rayon théorique (ou hSol pour plus de précision si dispo)
        // Simplification: On prend distToPlanet - TerreRadius pour l'altitude grossiere pour le drag
        // Atmosphère finit vers 140-200 unités
        float altitude = distToPlanet - TerreRadius;
        // float atmosphereHeight = 200.0f; 
        
        // SYSTEME DE DRAG DESACTIVÉ (Pour orbite pure sans ralentissement)
        /*
        if (altitude < atmosphereHeight) {
            // Densité lineaire (1 au sol, 0 en espace)
            float density = 1.0f - glm::clamp(altitude / atmosphereHeight, 0.0f, 1.0f);
            
            // Force de trainée : F = -0.5 * rho * v * v * Cd * A
            // Simplifié : F = -v * density * dragFactor
            // On veut que ça freine bien au sol
            float dragFactor = 2.0f; 
            
            glm::vec3 dragForce = -ironmanVelocity * density * dragFactor;
            ironmanForce += dragForce;
        }
        */

        // Integration Vitesse : V += (a_grav + a_thrust) * dt
        glm::vec3 acceleration = gravityDir * gravityAccel;
        acceleration += ironmanForce / ironmanMass; // Thrust
        
        ironmanVelocity += acceleration * dtSim;

        // Integration Position : P += V * dt
        ironmanPos += ironmanVelocity * dtSim;

        // Reset inputs
        ironmanForce = glm::vec3(0.0f);

        // 3. Collision avec le Sol (Planet Surface)
        glm::vec3 dirFromCenter = ironmanPos - terrePos;
        float currentDist = glm::length(dirFromCenter);
        glm::vec3 rayDir = glm::normalize(dirFromCenter); // Normale sortante (up)
        
        // Rotation pour heightmap (Doit matcher le rendu ~0.001f)
        float planetRotAnglePhysics = currentFrame * 0.001f; 
        
        glm::mat4 invRot = glm::rotate(glm::mat4(1.0f), -planetRotAnglePhysics, glm::vec3(0,1,0));
        glm::vec3 localRayDir = glm::vec3(invRot * glm::vec4(rayDir, 0.0f));
        
        float hSol = terre.getSurfaceHeight(localRayDir);
        float minAllowedDist = hSol + playerHeight;
        
        // [DEBUG LOGGING]
        static int logCounter = 0;
        if (logCounter++ % 60 == 0) {
            printf("\r[Phys] Alt: %.1f | Vel: %.1f | G: %.3f     ", 
                currentDist - hSol, glm::length(ironmanVelocity), gravityAccel);
        }

        if (currentDist < minAllowedDist) {
            // COLLISION SOL
            // a) Snap Position
            ironmanPos = terrePos + rayDir * minAllowedDist;
            
            // b) Gestion Vitesse
            float vDotN = glm::dot(ironmanVelocity, rayDir);
            
            if (vDotN < 0) { // Si on va VERS le sol
                // Annuler composante verticale
                ironmanVelocity -= vDotN * rayDir;
                // Friction
                ironmanVelocity *= 0.95f; 
                // Hard Stop si lent
                if (glm::length(ironmanVelocity) < 0.5f) {
                   ironmanVelocity = glm::vec3(0.0f);
                }
            }
        }
        
        lastPlanetPos = terrePos;

        // --- THIRD-PERSON CAMERA ORBIT (KSP Style) ---
        if (thirdPerson) {
             // ZOOM: Speed proportional to distance (logarithmic feel)
             // Min speed 10, Max speed unlimited?
             float zoomSpeed = glm::max(10.0f, camOrbitDist * 1.5f);
             
             if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) camOrbitDist -= deltaTime * zoomSpeed;
             if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) camOrbitDist += deltaTime * zoomSpeed;
             
             // Min limit only (don't clip inside player)
             if (camOrbitDist < 2.0f) camOrbitDist = 2.0f;
             // No Max Limit

             // Calcul position camera depuis Angles Orbitaux (Spheric to Cartesian)
             float yaw = glm::radians(camOrbitYaw);
             float pitch = glm::radians(camOrbitPitch);
             
             float hDist = camOrbitDist * cos(pitch);
             float vDist = camOrbitDist * sin(pitch);
             
             float offsetX = hDist * sin(yaw);
             float offsetZ = hDist * cos(yaw);
             float offsetY = vDist;
             
             glm::vec3 orbitPos = glm::vec3(offsetX, offsetY, offsetZ);

             // TARGET SELECTION (F3 Toggle)
             glm::vec3 targetPos = cameraTargetPlanet ? terrePos : ironmanPos;

             glm::vec3 potentialCamPos = targetPos + orbitPos;

             // --- COLLISION CAMERA SOL ---
             // Only check collision if we are close to the planet (to save perf and bugs when far away)
             // And if we are targeting the player (if targeting planet from far away, we don't collide)
             float distCamCenter = glm::length(potentialCamPos - terrePos);
             
             if (!cameraTargetPlanet || distCamCenter < (TerreRadius * 1.5f)) {
                 glm::vec3 dirCamCenter = potentialCamPos - terrePos;
                 glm::vec3 rayCamDesc = glm::normalize(dirCamCenter);
                 
                 float camRotAngle = currentFrame * 0.001f;
                 glm::mat4 invRotCam = glm::rotate(glm::mat4(1.0f), -camRotAngle, glm::vec3(0,1,0));
                 glm::vec3 localRayCam = glm::vec3(invRotCam * glm::vec4(rayCamDesc, 0.0f));
                 
                 float hCamSol = terre.getSurfaceHeight(localRayCam);
                 float minCamHeight = hCamSol + 2.0f; 
                 
                 if (distCamCenter < minCamHeight) {
                     potentialCamPos = terrePos + rayCamDesc * minCamHeight;
                     // Also correct orbitDist to match the collision
                     // So we don't "spring back" instantly if we zoom in
                     camOrbitDist = glm::length(potentialCamPos - targetPos);
                 }
             }

             camera.Position = potentialCamPos;
             camera.Front = glm::normalize(targetPos - camera.Position);
             
             camera.Right = glm::normalize(glm::cross(camera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
             camera.Up    = glm::normalize(glm::cross(camera.Right, camera.Front));
        }

        // --- RENDU ---
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glm::mat4 view = camera.GetViewMatrix();
        // Near plane légèrement augmenté pour reduire Z-fighting au loin
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width/(float)height, 0.01f, 2000000.0f);

        // Ombre
        glm::mat4 lightProjection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 2500.0f);
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.01f), terrePos, glm::vec3(0, 1, 0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glClear(GL_DEPTH_BUFFER_BIT);
        shaderShadow.use();
        
        // Rotation Planete Sychronisée
        float planetRotAngle = currentFrame * 0.0001f; // Vitesse reduite de x10 pour plus de calme
        
        glm::mat4 modelTerre = glm::translate(glm::mat4(1.0f), terrePos);
        modelTerre = glm::rotate(modelTerre, planetRotAngle, glm::vec3(0, 1, 0));
        
        shaderShadow.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shaderShadow.setMat4("model", modelTerre);
        terre.draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Final
        glViewport(0, 0, width, height);
        glClearColor(0.002f, 0.002f, 0.005f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (wireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Soleil
        shaderSoleil.use();
        // Diagnostic: distance camera <-> sun
        static int sunLog = 0;
        float distToSun = glm::length(camera.Position - glm::vec3(0.0f));
        if (++sunLog % 240 == 0) {
            std::cout << "[SUN DEBUG] Dist to Sun: " << distToSun << " | Sun radius: " << SoleilRadius << std::endl;
        }
        shaderSoleil.setMat4("model", glm::mat4(1.0f));
        shaderSoleil.setMat4("view", view);
        shaderSoleil.setMat4("projection", projection);
        // Draw sun without face culling in case normals or camera-inside cause it to be invisible
        GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullEnabled) glDisable(GL_CULL_FACE);
        soleil.draw();
        if (cullEnabled) glEnable(GL_CULL_FACE);

        // Additionally render a sky-sun billboard so the sun remains visible from the planet surface
        // Compute direction from camera to true sun (origin)
        glm::vec3 sunWorldPos = glm::vec3(0.0f);
        glm::vec3 sunDir = glm::normalize(sunWorldPos - camera.Position);
        // Place billboard along direction only; use view matrix without translation so it doesn't jitter
        float skyDistance = (std::max)(1000.0f, SoleilRadius * 10.0f);
        glm::mat4 sunSkyModel = glm::translate(glm::mat4(1.0f), sunDir * skyDistance);
        sunSkyModel = glm::scale(sunSkyModel, glm::vec3(SoleilRadius * 0.5f));

        // Build a view matrix without translation so the sky element stays fixed relative to camera orientation
        glm::mat4 viewNoTranslation = view;
        viewNoTranslation[3] = glm::vec4(0,0,0,1);

        // Draw sky sun without depth testing so it is always visible as a sky element
        glDisable(GL_DEPTH_TEST);
        shaderSoleil.setMat4("model", sunSkyModel);
        shaderSoleil.setMat4("view", viewNoTranslation);
        shaderSoleil.setMat4("projection", projection);
        soleil.draw();
        glEnable(GL_DEPTH_TEST);

        // Terre
        shaderPlanete.use();
        shaderPlanete.setMat4("model", modelTerre);
        shaderPlanete.setMat4("view", view);
        shaderPlanete.setMat4("projection", projection);
        shaderPlanete.setVec3("viewPos", camera.Position);
        
        // Shadow Map (Texture 1)
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        shaderPlanete.setInt("shadowMap", 1);

        // Planet Height/Detail Map (Texture 0)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, planetTexture);
        shaderPlanete.setInt("planetData", 0);

        // Cloud Shadow Map (Texture 2)
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, cloudTexture);
        shaderPlanete.setInt("cloudTexture", 2);

        // 3D Noise for accurate Cloud Shadows (Texture 3)
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_3D, volumeCloudTex);
        shaderPlanete.setInt("noiseTexture3D", 3);
        
        // Pass sync vars
        float cloudOffset = (currentFrame * 0.0001f) / (2.0f * 3.14159265f);
        shaderPlanete.setFloat("cloudRotationOffset", cloudOffset);
        shaderPlanete.setVec3("planetCenter", terrePos);
        shaderPlanete.setFloat("time", currentFrame);

        terre.draw();


        // Ironman (real model) scaled to playerUnit and rotated to face ironmanForward
            shaderModel.use();
            // determine local up (planet normal if in influence)
            glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);
            float distToPlanetVisual = glm::length(ironmanPos - terrePos);
            if (distToPlanetVisual < (TerreRadius * 5.0f)) camUp = glm::normalize(ironmanPos - terrePos);

            // Build an orthonormal basis where model's local forward aligns with ironmanForward
            glm::vec3 basisZ = glm::normalize(ironmanForward);
            glm::vec3 basisY = glm::normalize(camUp);
            glm::vec3 basisX = glm::normalize(glm::cross(basisY, basisZ));
            basisY = glm::normalize(glm::cross(basisZ, basisX));

            glm::mat4 rot(1.0f);
            rot[0] = glm::vec4(basisX, 0.0f);
            rot[1] = glm::vec4(basisY, 0.0f);
            rot[2] = glm::vec4(basisZ, 0.0f);

            glm::mat4 modelIron = glm::translate(glm::mat4(1.0f), ironmanPos);
            modelIron = modelIron * rot; // apply rotation
            // Apply an additional model-scale multiplier to normalize imported FBX size
            modelIron = glm::scale(modelIron, glm::vec3(playerUnit * ironmanModelScale));
            shaderModel.setMat4("model", modelIron);
            shaderModel.setMat4("view", view);
            shaderModel.setMat4("projection", projection);
            ironmanModel.Draw();

        // --- NUAGES ---
        if (!wireframeMode) {
             // Transparence : On ne veut pas écrire dans le Z-buffer pour ne pas cacher l'atmosphère derrière
            glDepthMask(GL_FALSE);
            
            // CORRECTION DOME/CLIPPING & VISIBILITE ESPACE:
            // Si on est dans l'espace (loins), on dessine les FACES AVANT (pour que le depth test passe DEVANT la planete)
            // Si on est dans l'atmosphere (dedans), on dessine les FACES ARRIERES (pour voir le volume de l'interieur)
            float distCam = glm::length(camera.Position - terrePos);
            // Seuil de transition : Rayon Terre + Max Altitude Nuage + Marge
            float transitionAlt = TerreRadius + 160.0f; 
            
            glEnable(GL_CULL_FACE);
            if (distCam > transitionAlt) { 
                // ESPACE : Vue exterieure -> On dessine la coque externe
                glCullFace(GL_BACK);
            } else {
                 // ATMOSPHERE/SOL : Vue interieure -> On dessine la coque interne
                 // On doit utiliser GL_LESS pour que les montagnes (Planete) cachent les nuages derriere elles.
                 // Le rayon (Z-Far) est suffisant (2M) pour voir la coque arriere.
                 glCullFace(GL_FRONT); 
            }
                 
            // Depth Mask False est correct pour la transparence
            glDepthMask(GL_FALSE);
            // Depth Test standard (LESS)
            glDepthFunc(GL_LESS);

            shaderNuages.use();
            
            // Depth Mask False est correct pour la transparence (pas d'ecriture Z, mais lecture Z active)
            glDepthMask(GL_FALSE);

            shaderNuages.use();

            shaderNuages.use();
            
            // Les nuages tournent un peu plus vite que la terre pour simuler le vent
            glm::mat4 modelNuages = glm::translate(glm::mat4(1.0f), terrePos);
            // On ne rotate pas le modele sphere nuage ici car le Raymarching gere sa propre rotation/UV
            // Mais pour que la boite englobante suive... en fait c une sphere parfaite donc la rotation ne change rien a la geometrie
            // Sauf si on veut que le repere local tourne. 
            // Pour le raymarching on prefere un repere stable aligné monde ou alors on passe la rotation.
            // On laisse l'identité (sauf translation)
            
            // L'échelle doit couvrir la zone max (Cloud Max Height)
            // Rayon Terre = 1000. Max Height = 140. Total = 1140. Base sphere ~1012.
            // Scale 1.15 => ~1163 radius. Suffisant pour englober tout le volume nuageux.
            modelNuages = glm::scale(modelNuages, glm::vec3(1.16f)); 

            shaderNuages.setMat4("model", modelNuages);
            shaderNuages.setMat4("view", view);
            shaderNuages.setMat4("projection", projection);
            shaderNuages.setVec3("lightPos", sunPos); 
            shaderNuages.setVec3("viewPos", camera.Position);
            shaderNuages.setVec3("planetCenter", terrePos);
            shaderNuages.setFloat("time", currentFrame);
            
            // SYNCHRONISATION ROTATION
            // Vitesse de rotation ultra-lente pour etre realiste
            // 0.00001f au lieu de 0.0001f (10x plus lent)
            // Correction finale: Valeur infime pour que ca paraisse fixe (pour test)
            float cloudOffset = (currentFrame * 0.000002f) / (2.0f * 3.14159265f);
            shaderNuages.setFloat("cloudRotationOffset", cloudOffset);
            
            shaderNuages.setFloat("planetRadius", TerreRadius);
            // On remonte les nuages pour qu'ils ne touchent plus le relief
            // Relief max = ~0.05 * 1000 = 50. donc on commence a 80
            shaderNuages.setFloat("cloudMinHeight", 90.0f); 
            shaderNuages.setFloat("cloudMaxHeight", 210.0f); // Altitude max augmentée significativement pour des tours géantes (etait 140)

             glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cloudTexture);
            shaderNuages.setInt("cloudCoverageMap", 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_3D, volumeCloudTex);
            shaderNuages.setInt("noiseTexture3D", 1);

            // Blend Mode : PRE-MULTIPLIED ALPHA (CORRECT VOLUMETRIC)
            // Use GL_ONE for Source because shader outputs (Color * Alpha).
            // Use GL_ONE_MINUS_SRC_ALPHA for Destination to let background show through (1 - Alpha).
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); 

            terreNuages.draw();

            // Reset Blend Mode standard
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Reset Depth Func standard
            glDepthFunc(GL_LESS);

            glCullFace(GL_BACK); // Rétablir le culling standard
            glDepthMask(GL_TRUE);
        }

        // Atmo
        if (!wireframeMode) {
            glDepthMask(GL_FALSE);
            
            // Pareil pour l'Atmosphere : Render Back Faces only pour eviter le clipping
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);

            shaderAtmosphere.use();
            glm::mat4 modelAtmo = glm::translate(glm::mat4(1.0f), terrePos);
            // CORRECTION: Atmosphere sphere plus grande pour englober les nuages et éviter le clipping visuel
            // Cloud max = 140. Radius=1000. Total=1140. Atmo > 1140.
            modelAtmo = glm::scale(modelAtmo, glm::vec3(1.25f)); // 1250 units radius (Augmenté pour fix clipping)

            shaderAtmosphere.setMat4("model", modelAtmo);
            shaderAtmosphere.setMat4("view", view);
            shaderAtmosphere.setMat4("projection", projection);
            
            shaderAtmosphere.setVec3("viewPos", camera.Position);
            shaderAtmosphere.setVec3("lightPos", sunPos); 
            shaderAtmosphere.setVec3("planetCenter", terrePos); // Ajout pour le shader v2
            shaderAtmosphere.setFloat("innerRadius", TerreRadius);
            shaderAtmosphere.setFloat("outerRadius", TerreRadius * 1.25f);

            terreAtmosphere.draw();
            
            glCullFace(GL_BACK); // Restore standard culling
            glDepthMask(GL_TRUE);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}