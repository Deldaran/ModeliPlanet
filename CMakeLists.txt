
cmake_minimum_required(VERSION 3.10)
project(ModeliPlanet)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)
    include_directories(dep/glad/include)
    add_executable(${PROJECT_NAME}
        src/main.cpp
        src/Model.cpp
        dep/glad/src/glad.c
    )
    target_link_libraries(${PROJECT_NAME} glfw glm::glm)
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
else()
    set(GLFW_DIR "C:/Users/dedjo/Documents/glfw")
    set(GLM_DIR "C:/Users/dedjo/Documents/glm")
    add_executable(${PROJECT_NAME}
        src/main.cpp
        src/Model.cpp
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
    # Find Assimp; if not present, fetch & build it automatically
    find_package(assimp QUIET)
    if (NOT assimp_FOUND)
        message(STATUS "Assimp not found on the system — downloading and building Assimp as part of the project")
        # reduce Assimp build footprint
        set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
        set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            assimp
            GIT_REPOSITORY https://github.com/assimp/assimp.git
            GIT_TAG v5.2.5
        )
        FetchContent_MakeAvailable(assimp)
        # After building, try to find the target
        if (TARGET assimp || TARGET assimp::assimp)
            message(STATUS "Assimp built via FetchContent")
        else()
            message(WARNING "Assimp was fetched but expected targets not found; attempting to continue")
        endif()
    endif()

    # Link any available assimp target
    if (TARGET assimp::assimp)
        target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)
    elseif (TARGET assimp)
        target_link_libraries(${PROJECT_NAME} PRIVATE assimp)
    else()
        if (assimp_FOUND)
            # use the imported package name if find_package succeeded earlier
            target_link_libraries(${PROJECT_NAME} PRIVATE ${assimp_LIBRARIES})
            target_include_directories(${PROJECT_NAME} PRIVATE ${assimp_INCLUDE_DIRS})
        else()
            message(WARNING "Assimp library not linked — model loading may fail. Install Assimp or provide it via vcpkg/CMAKE_PREFIX_PATH.")
        endif()
    endif()
endif()

# --- GESTION AUTOMATIQUE DES RESSOURCES (COPIE VERS DOSSIER EXE) ---
# Cette commande s'exécute APRES la compilation pour copier les fichiers à côté de l'exécutable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    # Copie aussi la Heightmap si elle existe à la racine
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/planet_heightmap_hd.png"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/planet_heightmap_hd.png"
    # Copie aussi les Nuages s'ils existent
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/planet_clouds.png"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/planet_clouds.png"
    COMMENT "Copie automatique des Shaders, Assets et Heightmap vers le dossier de l'executable"
)

# --- Tool: Heightmap Generator ---
add_executable(generate_heightmap tools/HeightmapGenerator.cpp)
target_include_directories(generate_heightmap PRIVATE src tools)

# --- Tool: Cloud Generator ---
add_executable(generate_clouds tools/CloudGenerator.cpp)
target_include_directories(generate_clouds PRIVATE src tools)

# --- Tool: 3D Noise Generator ---
add_executable(generate_noise3d tools/Generate3DNoise.cpp)
target_include_directories(generate_noise3d PRIVATE src tools)
