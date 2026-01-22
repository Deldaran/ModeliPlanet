#pragma once

// Classe de base pour la modélisation d'une planète
class Planet {
public:
    Planet();
    virtual ~Planet();
    // Méthodes pour générer la géométrie, afficher, etc.
    virtual void generateMesh() = 0;
    virtual void render() = 0;
};
