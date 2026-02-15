#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera() {
        UpdateVectors();
    }

    void SetPerspective(float fov, float aspect, float nearPlane, float farPlane) {
        m_Projection = glm::perspective(fov, aspect, nearPlane, farPlane);
        m_Projection[1][1] *= -1;  // Vulkan Y-flip
    }

    void ProcessMouseMovement(float xoffset, float yoffset) {
        xoffset *= m_MouseSensitivity;
        yoffset *= m_MouseSensitivity;

        m_Yaw += xoffset;
        m_Pitch += yoffset;

        // Constrain pitch
        if (m_Pitch > 89.0f) m_Pitch = 89.0f;
        if (m_Pitch < -89.0f) m_Pitch = -89.0f;

        UpdateVectors();
    }

    void ProcessKeyboard(int direction, float deltaTime) {
        float velocity = m_MovementSpeed * deltaTime;
        
        if (direction == 0) m_Position += m_Front * velocity;      // Forward (W)
        if (direction == 1) m_Position -= m_Front * velocity;      // Backward (S)
        if (direction == 2) m_Position -= m_Right * velocity;      // Left (A)
        if (direction == 3) m_Position += m_Right * velocity;      // Right (D)
        if (direction == 4) m_Position += m_Up * velocity;         // Up (Space)
        if (direction == 5) m_Position -= m_Up * velocity;         // Down (Ctrl)
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
    }

    glm::mat4 GetProjectionMatrix() const { return m_Projection; }
    glm::vec3 GetPosition() const { return m_Position; }
    
    void SetPosition(const glm::vec3& position) { m_Position = position; }
    void SetMovementSpeed(float speed) { m_MovementSpeed = speed; }
    void SetMouseSensitivity(float sensitivity) { m_MouseSensitivity = sensitivity; }
    
    float GetMovementSpeed() const { return m_MovementSpeed; }
    float GetMouseSensitivity() const { return m_MouseSensitivity; }

private:
    void UpdateVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        front.y = sin(glm::radians(m_Pitch));
        front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
        m_Front = glm::normalize(front);
        
        m_Right = glm::normalize(glm::cross(m_Front, glm::vec3(0.0f, 1.0f, 0.0f)));
        m_Up = glm::normalize(glm::cross(m_Right, m_Front));
    }

private:
    glm::mat4 m_Projection = glm::mat4(1.0f);
    glm::vec3 m_Position = glm::vec3(0.0f, 5.0f, 10.0f);
    glm::vec3 m_Front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 m_Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_Right = glm::vec3(1.0f, 0.0f, 0.0f);
    
    float m_Yaw = -90.0f;
    float m_Pitch = 0.0f;
    float m_MovementSpeed = 5.0f;
    float m_MouseSensitivity = 0.1f;
};