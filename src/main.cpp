#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>
#include "Planet.hpp"
#include "Shader.hpp"
#include "Camera.hpp"

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
float TerreRadius = 2.0f;
float SoleilRadius = 10.0f;
float planetDistance = 1000.0f;
float planetAngularSpeed = 0.1f; 

glm::vec3 lastPlanetPos(0.0f);
float baseCameraSpeed = 0.01f; 
float playerHeight = 0.01f; // hauteur du joueur au-dessus du sol

// --- Callbacks ---
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos; lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow *window, glm::vec3 terrePos) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    // Calcul de la distance pour adapter la vitesse
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

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) { wireframeMode = !wireframeMode; pKeyPressed = true; }
    }

    if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        camera.Position = terrePos + glm::vec3(0.0f, TerreRadius + 2.0f, 5.0f);
        std::cout << "Camera reset to above the planet." << std::endl;
    }
    if(glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) {
        showRay = !showRay;
        std::cout << "Show Ray: " << (showRay ? "ON" : "OFF") << std::endl;
    }
    //positionner autour de la terre
    if(glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
        camera.Position = terrePos + glm::vec3(0.0f, TerreRadius + 0.5f, 0.0f);
        std::cout << "Camera positioned above the planet surface." << std::endl;
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

    // --- Objets ---
    Planet soleil(SoleilRadius), terre(TerreRadius), terreAtmosphere(TerreRadius);
    soleil.generate(4); soleil.setupBuffers();
    terre.generate(8); terre.generateHeightMap(); terre.applyRectangularNoise(0.15f); terre.calculateNormals(); terre.setupBuffers();
    terreAtmosphere.generate(8); terreAtmosphere.setupBuffers();

    // --- Shaders ---
    Shader shaderSoleil("shaders/v_soleil.glsl", "shaders/f_soleil.glsl");
    Shader shaderPlanete("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
    Shader shaderShadow("shaders/v_shadow.glsl", "shaders/f_shadow.glsl");
    Shader shaderAtmosphere("shaders/atmo_vs.glsl", "shaders/atmo_fs.glsl");

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

        // --- SYSTEME D'ATTRACTION ET COLLISION ---
        glm::vec3 dirFromCenter = camera.Position - terrePos;
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
            std::cout << "\r[Attraction Active] Distance to Surface: " << (currentDist - TerreRadius) << "      " << std::flush;
            // Suivi du mouvement orbital
            glm::vec3 planetMovement = terrePos - lastPlanetPos;
            camera.Position += planetMovement; 

            // Collision précise par Raycast
            glm::vec3 rayDir = glm::normalize(dirFromCenter);
            glm::mat4 invRotInner = glm::rotate(glm::mat4(1.0f), -modelAngle, glm::vec3(0,1,0));
            glm::vec3 localRayDirInner = glm::vec3(invRotInner * glm::vec4(rayDir, 0.0f));
            float terrainHeight = terre.getSurfaceHeight(localRayDirInner);
            float minAllowedDist = terrainHeight + playerHeight;

            // Si la caméra pénètre le sol, on la replace exactement sur la surface
            if (currentDist < minAllowedDist) {
                camera.Position = terrePos + rayDir * minAllowedDist;
            }
        }
        lastPlanetPos = terrePos;

        // --- RENDU ---
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glm::mat4 view = camera.GetViewMatrix();
        // Near plane TRÈS petit pour ne pas clipper le sol
        glm::mat4 projection = glm::perspective(glm::radians(35.0f), (float)width/(float)height, 0.001f, 15000.0f);

        // Ombre
        glm::mat4 lightProjection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 2500.0f);
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.01f), terrePos, glm::vec3(0, 1, 0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glClear(GL_DEPTH_BUFFER_BIT);
        shaderShadow.use();
        glm::mat4 modelTerre = glm::translate(glm::mat4(1.0f), terrePos);
        modelTerre = glm::rotate(modelTerre, currentFrame * 0.02f, glm::vec3(0, 1, 0));
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
        shaderSoleil.setMat4("model", glm::mat4(1.0f));
        shaderSoleil.setMat4("view", view);
        shaderSoleil.setMat4("projection", projection);
        soleil.draw();

        // Terre
        shaderPlanete.use();
        shaderPlanete.setMat4("model", modelTerre);
        shaderPlanete.setMat4("view", view);
        shaderPlanete.setMat4("projection", projection);
        shaderPlanete.setVec3("viewPos", camera.Position);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        shaderPlanete.setInt("shadowMap", 1);
        terre.draw();

        // Atmo
        if (!wireframeMode) {
            glDepthMask(GL_FALSE);
            shaderAtmosphere.use();
            glm::mat4 modelAtmo = glm::translate(glm::mat4(1.0f), terrePos);
            modelAtmo = glm::scale(modelAtmo, glm::vec3(1.06f)); // Ajusté pour le relief
            shaderAtmosphere.setMat4("model", modelAtmo);
            shaderAtmosphere.setMat4("view", view);
            shaderAtmosphere.setMat4("projection", projection);
            shaderAtmosphere.setVec3("viewPos", camera.Position);
            terreAtmosphere.draw();
            glDepthMask(GL_TRUE);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}