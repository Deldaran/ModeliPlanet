#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Planet.hpp"
#include "Shader.hpp"
#include "Camera.hpp"

// Variables Globales
Camera camera(glm::vec3(0.0f, 2.0f, 15.0f));
float lastX = 400, lastY = 300;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool wireframeMode = true; 
bool pKeyPressed = false;

// Callbacks
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; 
    lastX = xpos; lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.ProcessKeyboard("FORWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.ProcessKeyboard("BACKWARD", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.ProcessKeyboard("LEFT", deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.ProcessKeyboard("RIGHT", deltaTime);

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (!pKeyPressed) {
            wireframeMode = !wireframeMode;
            if (wireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            pKeyPressed = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) pKeyPressed = false;
}

int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 

    GLFWwindow* window = glfwCreateWindow(800, 600, "Systeme Solaire Shadows", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST); 

    // --- OBJETS ---
    Planet soleil, terre;

    camera.Position = glm::vec3(0.0f, 0.0f, 20.0f);
    camera.Yaw = -90.0f; 
    camera.Pitch = 0.0f;
    camera.updateCameraVectors();
    soleil.generate(4);
    soleil.setupBuffers();

    terre.generate(5);
    terre.generateHeightMap();
    terre.applyRectangularNoise(0.15f);
    terre.calculateNormals(); // Crucial pour les ombres !
    terre.setupBuffers();

    // --- SHADERS ---
    Shader shaderSoleil("shaders/v_soleil.glsl", "shaders/f_soleil.glsl");
    Shader shaderPlanete("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
    Shader shaderShadow("shaders/v_shadow.glsl", "shaders/f_shadow.glsl");

    // --- SHADOW MAP CONFIG ---
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
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

        // --- CALCUL DES POSITIONS ---
        glm::vec3 terrePos = glm::vec3(sin(glfwGetTime() * 0.3f) * 10.0f, 0.0f, cos(glfwGetTime() * 0.3f) * 10.0f);
        glm::mat4 modelTerre = glm::mat4(1.0f);
        modelTerre = glm::translate(modelTerre, terrePos);
        modelTerre = glm::rotate(modelTerre, (float)glfwGetTime() * 0.8f, glm::vec3(0, 1, 0));

        // 1. PASSE D'OMBRE (Depuis le soleil vers la terre)
        glm::mat4 lightProjection = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 30.0f);
        glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), terrePos, glm::vec3(0, 1, 0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        shaderShadow.use();
        shaderShadow.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        shaderShadow.setMat4("model", modelTerre);
        terre.draw();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. PASSE FINALE
        int width, height;
        glfwGetFramebufferSize(window, &width, &height); // Récupère la taille réelle (ex: 1600x1200 sur Retina)
        glViewport(0, 0, width, height); 

        glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // On remet le mode fil de fer SEULEMENT ici si activé
        if (wireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // Dessin Soleil
        shaderSoleil.use();
        glm::mat4 modelSoleil = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        shaderSoleil.setMat4("model", modelSoleil);
        shaderSoleil.setMat4("view", view);
        shaderSoleil.setMat4("projection", projection);
        glDisable(GL_CULL_FACE);
        soleil.draw();
        glEnable(GL_CULL_FACE);

        // Dessin Terre
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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}