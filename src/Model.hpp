#pragma once

#include <string>
#include <vector>
#include <map>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct MeshVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    unsigned int VAO, VBO, EBO;
    Mesh() : VAO(0), VBO(0), EBO(0) {}
    void setupMesh();
    void Draw();
};

class Model {
public:
    Model(const std::string& path);
    ~Model();
    void Draw();
private:
    std::vector<Mesh> meshes;
    std::string directory;
    std::vector<Texture> textures_loaded;

    void loadModel(const std::string& path);
    void processNode(struct aiNode* node, const struct aiScene* scene);
    Mesh processMesh(struct aiMesh* mesh, const struct aiScene* scene);
    std::vector<Texture> loadMaterialTextures(struct aiMaterial* mat, int type, const std::string& typeName);
    unsigned int TextureFromFile(const char* path, const std::string &directory);
};
