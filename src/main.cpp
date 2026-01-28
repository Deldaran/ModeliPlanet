#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Planet.hpp"
#include "Shader.hpp"
#include "Camera.hpp"

// --- Variables Globales ---
Camera camera(glm::vec3(0.0f, 2.0f, 15.0f));
float lastX = 400, lastY = 300;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool wireframeMode = false; 
bool pKeyPressed = false;

// Paramètres de l'univers
float TerreRadius = 2.0f;
float SoleilRadius = 10.0f;
float planetDistance = 1000.0f;
float planetAngularSpeed = 0.1f; 

// Système de Focus
int cameraFocus = 0; // 0=Libre, 1=Soleil, 2=Terre
glm::vec3 lastPlanetPos(0.0f);
float baseCameraSpeed = 5.0f;

// --- Callbacks ---
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos; lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);

    float speedMultiplier = 1.0f;
    if (cameraFocus == 1) speedMultiplier = SoleilRadius * 2.0f;
    if (cameraFocus == 2) speedMultiplier = TerreRadius * 3.0f;
    
    float currentSpeed = baseCameraSpeed * speedMultiplier * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard("FORWARD", currentSpeed);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard("BACKWARD", currentSpeed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard("LEFT", currentSpeed);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard("RIGHT", currentSpeed);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        currentSpeed *= 2.0f;
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) {
            wireframeMode = !wireframeMode;
            pKeyPressed = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) pKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) cameraFocus = 0;
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) cameraFocus = 1;
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) cameraFocus = 2;
}

int main() {
    // --- Initialisation GLFW/GLAD ---
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "Planet Engine", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // --- Configuration Globale OpenGL ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); 
    glCullFace(GL_BACK);    
    glFrontFace(GL_CCW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Objets ---
    Planet soleil(SoleilRadius), terre(TerreRadius), terreAtmosphere(TerreRadius);
    
    soleil.generate(4);
    soleil.setupBuffers();

    terre.generate(8);
    terre.generateHeightMap();
    terre.applyRectangularNoise(0.15f); // Relief
    terre.calculateNormals();
    terre.setupBuffers();

    terreAtmosphere.generate(8); // Sphère lisse pour l'atmo
    terreAtmosphere.setupBuffers();

    // --- Shaders ---
    Shader shaderSoleil("shaders/v_soleil.glsl", "shaders/f_soleil.glsl");
    Shader shaderPlanete("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
    Shader shaderShadow("shaders/v_shadow.glsl", "shaders/f_shadow.glsl");
    Shader shaderAtmosphere("shaders/atmo_vs.glsl", "shaders/atmo_fs.glsl");

    // --- Shadow Map Config ---
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    const unsigned int SHADOW_WIDTH = 2048, SHADOW_HEIGHT = 2048;
    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Position de la Terre
        glm::vec3 terrePos = glm::vec3(
            sin(currentFrame * planetAngularSpeed) * planetDistance, 
            0.0f, 
            cos(currentFrame * planetAngularSpeed) * planetDistance
        );

        // Camera Follow
        if (cameraFocus == 2) {
            glm::vec3 planetMovement = terrePos - lastPlanetPos;
            camera.Position += planetMovement;
        }
        lastPlanetPos = terrePos;

        glm::mat4 view = camera.GetViewMatrix();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100000.0f);

        // --- PASSE D'OMBRE ---
        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 2000.0f);
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.01f), terrePos, glm::vec3(0, 1, 0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        shaderShadow.use();
        glm::mat4 modelTerre = glm::translate(glm::mat4(1.0f), terrePos);
        modelTerre = glm::rotate(modelTerre, currentFrame * 0.8f, glm::vec3(0, 1, 0));
        shaderShadow.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shaderShadow.setMat4("model", modelTerre);
        terre.draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // --- PASSE FINALE ---
        glViewport(0, 0, width, height); 
        glClearColor(0.005f, 0.005f, 0.01f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (wireframeMode) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_CULL_FACE); 
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
        }

        // 1. DESSIN SOLEIL
        shaderSoleil.use();
        glm::mat4 modelSoleil = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)); 
        shaderSoleil.setMat4("model", modelSoleil);
        shaderSoleil.setMat4("view", view);
        shaderSoleil.setMat4("projection", projection);
        soleil.draw();

        // 2. DESSIN TERRE (Opaque)
        shaderPlanete.use();
        shaderPlanete.setMat4("model", modelTerre);
        shaderPlanete.setMat4("view", view);
        shaderPlanete.setMat4("projection", projection);
        shaderPlanete.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shaderPlanete.setVec3("lightPos", glm::vec3(0.0f));
        shaderPlanete.setVec3("viewPos", camera.Position);
        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        shaderPlanete.setInt("shadowMap", 1);
        terre.draw();

        // 3. DESSIN ATMOSPHÈRE (Transparent - Après la terre)
        if (!wireframeMode) {
            glDepthMask(GL_FALSE); // Désactive l'écriture Z-buffer
            shaderAtmosphere.use();
            glm::mat4 modelAtmo = glm::translate(glm::mat4(1.0f), terrePos);
            // On scale légèrement l'objet lisse pour couvrir le relief
            modelAtmo = glm::scale(modelAtmo, glm::vec3(0.6f)); 

            shaderAtmosphere.setMat4("model", modelAtmo);
            shaderAtmosphere.setMat4("view", view);
            shaderAtmosphere.setMat4("projection", projection);
            shaderAtmosphere.setVec3("viewPos", camera.Position);
            shaderAtmosphere.setVec3("lightPos", glm::vec3(0.0f));

            terreAtmosphere.draw();
            glDepthMask(GL_TRUE); // Réactive le Z-buffer
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}