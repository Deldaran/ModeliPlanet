#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
    public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw = -90.0f;
    float Pitch = 0.0f;
    float MovementSpeed = 2.5f;
    float MouseSensitivity = 0.1f;

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f)) 
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)), WorldUp(glm::vec3(0.0f, 1.0f, 0.0f)) {
        Position = position;
        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix() {
        return glm::lookAt(Position, Position + Front, Up);
    }

    // Accept a precomputed velocity (distance to move) rather than deltaTime.
    void ProcessKeyboard(const std::string& direction, float velocity) {
        if (direction == "FORWARD")  Position += Front * velocity;
        if (direction == "BACKWARD") Position -= Front * velocity;
        if (direction == "LEFT")     Position -= Right * velocity;
        if (direction == "RIGHT")    Position += Right * velocity;
    }

    void ProcessMouseMovement(float xoffset, float yoffset) {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw   += xoffset;
        Pitch += yoffset;

        if (Pitch > 89.0f)  Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;

        updateCameraVectors();
    }

    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up    = glm::normalize(glm::cross(Right, Front));
    }

};
#endif // CAMERA_HPP
