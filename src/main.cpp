#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
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

// Paramètres de l'univers
// Unité du joueur (1.0f)
float playerUnit = 1.0f; // taille de l'entité (1f)
// Ne pas modifier les planètes automatiquement ici — garder leurs tailles d'origine
float TerreRadius = 1000.0f; // PLANETE GEANTE (X10 par rapport a 90)
float SoleilRadius = 5000.0f; // Soleil plus grand aussi
float planetDistance = 20000.0f; // Plus loin
// Ralentissement global pour effet "Realiste"
// 0.01 c'etait ~10min par orbite. On passe a 0.0005 (~3h par orbite)
float planetAngularSpeed = 0.0005f; 

glm::vec3 lastPlanetPos(0.0f);
float baseCameraSpeed = 5.0f; // Vitesse de caméra augmentée pour échelle géante
float playerHeight = 0.01f; // hauteur du joueur au-dessus du sol
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

// Camera Orbital State
float camOrbitYaw = 0.0f;
float camOrbitPitch = 20.0f;
float camOrbitDist = 5.0f;

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

    // In third-person mode, control the ironman (flight). Otherwise control the free camera.
    if (thirdPerson) {
        float moveSpeed = ironmanSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) moveSpeed *= 2.0f;

        // La direction "Avant" du joueur dépend de la caméra (pour que W aille "au fond de l'ecran")
        // On projette le vecteur avant de la camera sur le plan horizontal (XZ local du joueur si gravité ?)
        // Simplification: On utilise Camera Front projetée
        glm::vec3 camDir = camera.Front;
        
        // Si on veut contrôle type AVION/KSP EVA :
        // W = Avancer dans la direction où regarde le joueur ? 
        // User demande "KSP quand on est avec un kerbal" (EVA)
        // -> ZQSD déplace le kerbal par rapport à la caméra. 
        // -> Espace monte, Ctrl descend.
        // -> La caméra tourne autour librement.
        // -> Le kerbal s'oriente vers le mouvement.
        
        glm::vec3 moveDir(0.0f);
        glm::vec3 camRight = camera.Right;
        glm::vec3 camFront = camera.Front;
     
        // On veut bouger 'à plat' par rapport à la caméra si possible, ou en 3D
        // EVA spatial = 3D.
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += camFront;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= camFront;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= camRight;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += camRight;
        
        // Space / Ctrl (Up / Down global ou local ?)
        // KSP EVA a un Jetpack. Space = Up (relatif camera up ?)
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) moveDir += camera.Up;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) moveDir -= camera.Up;

        if (glm::length(moveDir) > 0.01f) {
            moveDir = glm::normalize(moveDir);
            ironmanPos += moveDir * moveSpeed;
            
            // Le perso regarde dans la direction du mouvement (slerp pour lissage ?)
            ironmanForward = moveDir; 
            // Update Pitch/Yaw for rendering model correctly
            // (Simple LookAt direction)
        }

    } else {
        // Calcul de la distance pour adapter la vitesse (free camera)
        float d = glm::length(camera.Position - terrePos);
        float distToSurfaceFactor = glm::clamp((d - TerreRadius) * 0.5f, 0.02f, 1.0f);
        
        float speedMultiplier = 10.0f;
        if (d < TerreRadius * 5.0f) speedMultiplier = TerreRadius * 2.0f;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speedMultiplier *= 4.0f;

        float currentSpeed = baseCameraSpeed * speedMultiplier * distToSurfaceFactor * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard("FORWARD", currentSpeed);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard("BACKWARD", currentSpeed);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard("LEFT", currentSpeed);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard("RIGHT", currentSpeed);
    }

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) { wireframeMode = !wireframeMode; pKeyPressed = true; }
    }

    if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        // Reset free camera position above the planet
        camera.Position = terrePos + glm::vec3(0.0f, TerreRadius + playerUnit * 2.0f, 5.0f);
        std::cout << "Camera reset to above the planet." << std::endl;
    }
    if(glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        showRay = !showRay;
        std::cout << "Show Ray: " << (showRay ? "ON" : "OFF") << std::endl;
    }
    //positionner autour de la terre
    if(glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
        // Teleport the player (Ironman) to a point above the planet surface and make camera follow
        ironmanPos = terrePos + glm::vec3(0.0f, TerreRadius + playerUnit * 0.5f, 0.0f);
        // Recompute desired camera position immediately so it snaps behind the player
        glm::vec3 desiredCam = ironmanPos - ironmanForward * cameraFollowDistance + glm::vec3(0.0f, cameraFollowHeight, 0.0f);
        camera.Position = desiredCam;
        camera.Front = glm::normalize(ironmanPos - camera.Position);
        camera.Right = glm::normalize(glm::cross(camera.Front, camera.WorldUp));
        camera.Up = glm::normalize(glm::cross(camera.Right, camera.Front));
        std::cout << "Player teleported above the planet surface." << std::endl;
    }
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1024, 768, "Planet Engine - Auto Capture", NULL, NULL);
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
    ironmanPos = startPlanetPos + glm::vec3(0.0f, TerreRadius + 5.0f, 0.0f);
    
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
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glm::vec3 terrePos = glm::vec3(
            sin(currentFrame * planetAngularSpeed) * planetDistance, 
            0.0f, 
            cos(currentFrame * planetAngularSpeed) * planetDistance
        );

        processInput(window, terrePos);

        // --- SYSTEME D'ATTRACTION ET COLLISION (APPLIQUÉ AU JOUEUR) ---
        glm::vec3 dirFromCenter = ironmanPos - terrePos;
        float currentDist = glm::length(dirFromCenter);
        glm::vec3 rayDir = glm::normalize(dirFromCenter);
        // Prendre en compte la rotation appliquée au modelTerre lors du rendu :
        // on convertit la direction monde en direction locale (inverse rotation)
        float modelAngle = currentFrame * 0.02f;
        glm::mat4 invRot = glm::rotate(glm::mat4(1.0f), -modelAngle, glm::vec3(0,1,0));
        glm::vec3 localRayDir = glm::vec3(invRot * glm::vec4(rayDir, 0.0f));
        float hSol = terre.getSurfaceHeight(localRayDir);
        float deltaSurface = currentDist - hSol;

        static int logCounter = 0;
        if (logCounter++ % 100 == 0) {
            // Distance to Surface devient dynamique
            std::cout << "\r[DEBUG] Dist: " << currentDist 
                    << " | Sol: " << hSol 
                    << " | Delta: " << deltaSurface << "      " << std::flush;
        }
        
        // Rayon d'influence (Capture la caméra si elle est proche)
        float influenceRadius = TerreRadius * 5.0f; 

        if (currentDist < influenceRadius) {
            std::cout << "\r[Attraction Active] Player Dist to Surface: " << (currentDist - TerreRadius) << "      " << std::flush;
            // Suivi du mouvement orbital appliqué au joueur
            glm::vec3 planetMovement = terrePos - lastPlanetPos;
            ironmanPos += planetMovement;

            // Collision précise par Raycast (sur le joueur)
            glm::vec3 rayDirInner = glm::normalize(dirFromCenter);
            glm::mat4 invRotInner = glm::rotate(glm::mat4(1.0f), -modelAngle, glm::vec3(0,1,0));
            glm::vec3 localRayDirInner = glm::vec3(invRotInner * glm::vec4(rayDirInner, 0.0f));
            float terrainHeight = terre.getSurfaceHeight(localRayDirInner);
            float minAllowedDist = terrainHeight + playerHeight;

            // Si le joueur pénètre le sol, on le replace exactement sur la surface
            if (currentDist < minAllowedDist) {
                ironmanPos = terrePos + rayDirInner * minAllowedDist;
            }

            // Si le joueur est très proche de la surface on lui applique la rotation propre de la planète
            if (std::abs(currentDist - minAllowedDist) < groundRotationThreshold) {
                // angle de rotation depuis la frame précédente
                float rotationDelta = deltaTime * planetSelfRotationSpeed;
                glm::mat4 rot = glm::rotate(glm::mat4(1.0f), rotationDelta, glm::vec3(0,1,0));

                // Rotate player position around planet center
                glm::vec3 localPos = ironmanPos - terrePos;
                localPos = glm::vec3(rot * glm::vec4(localPos, 1.0f));
                ironmanPos = terrePos + localPos;

                // Rotate player forward vector so the player 'turns' with the ground
                ironmanForward = glm::normalize(glm::vec3(rot * glm::vec4(ironmanForward, 0.0f)));
                // Recompute yaw/pitch from forward vector
                ironmanYaw = glm::degrees(atan2(ironmanForward.z, ironmanForward.x));
                ironmanPitch = glm::degrees(asin(glm::clamp(ironmanForward.y, -1.0f, 1.0f)));
            }
        }
        lastPlanetPos = terrePos;

        // --- THIRD-PERSON CAMERA ORBIT (KSP Style) ---
        if (thirdPerson) {
             // Scroll pour zoomer ? (Pas implementé callback scroll ici mais on peut map I/K ou +/-)
             if (glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) camOrbitDist -= deltaTime * 10.0f;
             if (glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) camOrbitDist += deltaTime * 10.0f;
             if (camOrbitDist < 2.0f) camOrbitDist = 2.0f;
             if (camOrbitDist > 50.0f) camOrbitDist = 50.0f;

             // Calcul position camera depuis Angles Orbitaux (Spheric to Cartesian)
             // Base locale autour du joueur
             float yaw = glm::radians(camOrbitYaw);
             float pitch = glm::radians(camOrbitPitch);
             
             // Offset camera position
             float hDist = camOrbitDist * cos(pitch);
             float vDist = camOrbitDist * sin(pitch);
             
             float offsetX = hDist * sin(yaw);
             float offsetZ = hDist * cos(yaw);
             float offsetY = vDist;
             
             // La position brute par rapport à l'univers (sans rotation locale du joueur)
             glm::vec3 orbitPos = glm::vec3(offsetX, offsetY, offsetZ);

             camera.Position = ironmanPos + orbitPos;
             camera.Front = glm::normalize(ironmanPos - camera.Position);
             
             // Recalculer Up pour rester stable
             // Dans KSP EVA, le UP de la camera tend vers le UP de l'espace ou de la planete ? 
             // Le WorldUp est Y, donc ca va.
             camera.Right = glm::normalize(glm::cross(camera.Front, glm::vec3(0.0f, 1.0f, 0.0f)));
             camera.Up    = glm::normalize(glm::cross(camera.Right, camera.Front));
        }

        // --- RENDU ---
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glm::mat4 view = camera.GetViewMatrix();
        // Near plane légèrement augmenté pour reduire Z-fighting au loin
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width/(float)height, 0.1f, 2000000.0f);

        // Ombre
        glm::mat4 lightProjection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 2500.0f);
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.01f), terrePos, glm::vec3(0, 1, 0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glClear(GL_DEPTH_BUFFER_BIT);
        shaderShadow.use();
        
        // Rotation Planete Sychronisée
        float planetRotAngle = currentFrame * 0.001f; // Vitesse reduite (etait 0.02)
        
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
        shaderPlanete.setInt("planetData", 0); // On passera ça dans le shader

        terre.draw();


        // Ironman (real model) scaled to playerUnit and rotated to face ironmanForward
            shaderModel.use();
            // determine local up (planet normal if in influence)
            glm::vec3 camUp = glm::vec3(0.0f, 1.0f, 0.0f);
            float distToPlanet = glm::length(ironmanPos - terrePos);
            if (distToPlanet < (TerreRadius * 5.0f)) camUp = glm::normalize(ironmanPos - terrePos);

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
            glDisable(GL_CULL_FACE); // Visible de l'interieur

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
            shaderNuages.setFloat("time", (float)glfwGetTime());
            
            // SYNCHRONISATION ROTATION
            // On calcule l'offset UV (0..1) correspondant a l'angle de rotation (0..2PI)
            // angle = currentFrame * 0.001
            float cloudOffset = (currentFrame * 0.001f) / (2.0f * 3.14159265f);
            shaderNuages.setFloat("cloudRotationOffset", cloudOffset);
            
            shaderNuages.setFloat("planetRadius", TerreRadius);
            // On remonte les nuages pour qu'ils ne touchent plus le relief
            // Relief max = ~0.05 * 1000 = 50. donc on commence a 80
            shaderNuages.setFloat("cloudMinHeight", 80.0f); // Altitude min augmentée (etait 10)
            shaderNuages.setFloat("cloudMaxHeight", 140.0f); // Altitude max (etait 50)

             glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cloudTexture);
            shaderNuages.setInt("cloudCoverageMap", 0);

            // Blend Mode specifique pour l'accumulation
            // Volumetric clouds output premultiplied alpha-ish
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 

            terreNuages.draw();

            glEnable(GL_CULL_FACE);
            glDepthMask(GL_TRUE);
        }

        // Atmo
        if (!wireframeMode) {
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE); // IMPORTANT : Désactiver le culling pour voir l'intérieur

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
            
            glEnable(GL_CULL_FACE); // Réactiver
            glDepthMask(GL_TRUE);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}