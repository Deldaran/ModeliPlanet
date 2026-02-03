#include "Model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Simple mesh methods
void Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, Position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, Normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), (void*)offsetof(MeshVertex, TexCoords));

    glBindVertexArray(0);
}

void Mesh::Draw() {
    // bind diffuse texture if present
    if (!textures.empty()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0].id);
    }
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ---------------- Model ----------------
Model::Model(const std::string& path) {
    loadModel(path);
}

Model::~Model() {
    for (auto &m : meshes) {
        if (m.EBO) glDeleteBuffers(1, &m.EBO);
        if (m.VBO) glDeleteBuffers(1, &m.VBO);
        if (m.VAO) glDeleteVertexArrays(1, &m.VAO);
        for (auto &t : m.textures) if (t.id) glDeleteTextures(1, &t.id);
    }
}

void Model::Draw() {
    for (auto &m : meshes) m.Draw();
}

void Model::loadModel(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }
    directory = path.substr(0, path.find_last_of('/'));
    if (directory.empty()) directory = path.substr(0, path.find_last_of('\\'));
    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    for (unsigned int i = 0; i < node->mNumChildren; ++i) processNode(node->mChildren[i], scene);
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    Mesh outMesh;
    std::string meshName = mesh->mName.C_Str();
    // normalize name to lower-case for checks
    std::string lname = meshName;
    std::transform(lname.begin(), lname.end(), lname.begin(), [](unsigned char c){ return std::tolower(c); });
    // Skip common helper/locator meshes exported from modelling tools
    if (lname.find("sphere") != std::string::npos || lname.find("cylinder") != std::string::npos ||
        lname.find("camera") != std::string::npos || lname.find("circle") != std::string::npos ||
        lname.find("null") != std::string::npos || lname.find("root") != std::string::npos ||
        lname.find("empty") != std::string::npos || lname.find("locator") != std::string::npos) {
        std::cout << "Model: skipping helper mesh '" << meshName << "'" << std::endl;
        return outMesh; // return empty mesh (no geometry)
    }
    if (mesh->mNumVertices == 0) {
        std::cout << "Model: skipping empty mesh '" << meshName << "'" << std::endl;
        return outMesh;
    }
    std::cout << "Model: loading mesh '" << meshName << "' (v=" << mesh->mNumVertices << ", f=" << mesh->mNumFaces << ")" << std::endl;
    outMesh.vertices.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        MeshVertex v;
        v.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
        if (mesh->HasNormals()) v.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        else v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        if (mesh->mTextureCoords[0]) v.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        else v.TexCoords = glm::vec2(0.0f);
        outMesh.vertices.push_back(v);
    }
    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        aiFace face = mesh->mFaces[f];
        for (unsigned int k = 0; k < face.mNumIndices; ++k) outMesh.indices.push_back(face.mIndices[k]);
    }
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        auto diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        outMesh.textures.insert(outMesh.textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    }
    outMesh.setupMesh();
    return outMesh;
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, int type, const std::string& typeName) {
    std::vector<Texture> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount((aiTextureType)type); ++i) {
        aiString str;
        mat->GetTexture((aiTextureType)type, i, &str);
        std::string path = str.C_Str();
        bool skip = false;
        for (auto &t : textures_loaded) if (t.path == path) { textures.push_back(t); skip = true; break; }
        if (skip) continue;
        unsigned int texID = 0;
        // If texture is embedded in the file, Assimp may give a path like "*0"
        if (!path.empty() && path[0] == '*') {
            int texIndex = atoi(path.c_str() + 1);
            // embedded textures not handled in this simplified loader
            // fallback: skip complex embedded loading here
            (void)texIndex;
        } else {
            texID = TextureFromFile(path.c_str(), directory);
        }
        Texture tex;
        tex.id = texID;
        tex.type = typeName;
        tex.path = path;
        textures.push_back(tex);
        textures_loaded.push_back(tex);
    }
    return textures;
}

unsigned int Model::TextureFromFile(const char* path, const std::string &directory) {
    std::string filename = std::string(path);
    std::string full = directory + "/" + filename;
    int width, height, nrChannels;
    unsigned char* data = stbi_load(full.c_str(), &width, &height, &nrChannels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << full << std::endl;
        return 0;
    }
    GLenum format = GL_RGB;
    if (nrChannels == 1) format = GL_RED;
    else if (nrChannels == 3) format = GL_RGB;
    else if (nrChannels == 4) format = GL_RGBA;
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    return textureID;
}
