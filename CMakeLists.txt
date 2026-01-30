
cmake_minimum_required(VERSION 3.10)
project(ModeliPlanet)
set(CMAKE_CXX_STANDARD 17)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)
    include_directories(dep/glad/include)
    add_executable(${PROJECT_NAME} 
        src/main.cpp 
        dep/glad/src/glad.c
    )
    target_link_libraries(${PROJECT_NAME} glfw glm::glm)
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
else()
    set(GLFW_DIR "C:/Users/dedjo/Documents/glfw")
    set(GLM_DIR "C:/Users/dedjo/Documents/glm")
    add_executable(${PROJECT_NAME}
        src/main.cpp
        dep/glad/src/glad.c
    )
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${GLFW_DIR}/include
        ${GLM_DIR}
        dep/glad/include
        src
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        "${GLFW_DIR}/lib-vc2019/glfw3.lib"
        opengl32
    )
endif()

# --- GESTION DES SHADERS ET ASSETS ---
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(SHADER_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_TARGET_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADER_SOURCE_DIR}" "${SHADER_TARGET_DIR}"
    COMMENT "Copie des shaders vers le dossier d'exécution..."
)
add_dependencies(${PROJECT_NAME} copy_shaders)