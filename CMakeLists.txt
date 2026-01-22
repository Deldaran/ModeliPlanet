cmake_minimum_required(VERSION 3.10)
project(ModeliPlanet)

set(CMAKE_CXX_STANDARD 17)


list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

include_directories(dep/glad/include)

add_executable(${PROJECT_NAME} 
    src/main.cpp 
    dep/glad/src/glad.c
)

target_link_libraries(${PROJECT_NAME} glfw glm::glm)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")

# --- GESTION DES SHADERS ---

# On définit où sont les shaders sources et où ils doivent aller
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(SHADER_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# Commande pour créer le dossier et copier les fichiers à chaque compilation
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_TARGET_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_SOURCE_DIR} ${SHADER_TARGET_DIR}
    COMMENT "Mise à jour des fichiers shaders dans le dossier build..."
)

# On force l'exécutable à attendre que la copie soit finie avant de se compiler
add_dependencies(${PROJECT_NAME} copy_shaders)
endif()