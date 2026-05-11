#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/graphics/al_Font.hpp"
#include "al/math/al_Random.hpp"
#include <cmath>
#include <fstream>
#include <vector>

using namespace al;
using namespace std;

// Helper: random 3D vector
Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}

struct AlloApp : App {

  // GUI parameters
  Parameter pointSize{"/pointSize", "", 5.0, 1.0, 10.0};
  Parameter timeStep{"/timeStep", "", 0.02, 0.01, 0.6};
  Parameter dragforce{"/dragforce", "", 0.5, 0.0, 1.0};
  Parameter stiffness{"/stiffness", "", 1.0, 0.1, 5.0};
  Parameter sphereRadius{"/sphereRadius", "", 10.0, 1.0, 20.0};

  // Font for rendering text
  Font font;

  // Simulation data (same as your point system)
  Mesh mesh;                 // holds positions (vertices)
  vector<Vec3f> velocity;   // velocity of each particle
  vector<Vec3f> force;      // accumulated force
  vector<float> mass;       // mass of each particle

  // Text strings (one per particle)
  vector<string> words = {
      "ONE", "TWO", "THREE", "FOUR", "FIVE",
      "SIX", "SEVEN", "EIGHT", "NINE", "TEN"
  };

  // One mesh per text string
  vector<Mesh> textMeshes;

  // GUI setup
  void onInit() override {
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();

    gui.add(pointSize);
    gui.add(timeStep);
    gui.add(dragforce);
    gui.add(stiffness);
    gui.add(sphereRadius);
  }

  void onCreate() override {

    // Load font
    font.load("data/VeraMono.ttf", 200, 1024);
    font.alignCenter();

    mesh.primitive(Mesh::POINTS);

    // IMPORTANT: number of particles = number of words
    int numberPoints = words.size();

    for (int i = 0; i < numberPoints; i++) {

      // Create random position on sphere
      Vec3f sphere = randomVec3f(1.0);
      sphere.normalize(sphereRadius);

      // Store position in mesh (this drives motion)
      mesh.vertex(sphere);

      // Initialize physics state
      velocity.push_back(randomVec3f(0.1));
      force.push_back(Vec3f(0, 0, 0));
      mass.push_back(1.0);

      // --- TEXT PART ---
      // Create a mesh for this word
      Mesh textMesh;
      font.write(textMesh, words[i].c_str(), 0.2f);

      // Store it
      textMeshes.push_back(textMesh);
    }

    // Move camera back
    nav().pos(0, 0, 40);
  }

  void onAnimate(double dt) override {

    // Access positions stored in mesh
    vector<Vec3f> &position = mesh.vertices();

    for (int i = 0; i < position.size(); i++) {

      // Direction from origin → point
      Vec3f direction = position[i];
      float distance = direction.mag();

      if (distance > 0.0001) {
        direction.normalize();

        // Spring toward sphere radius
        float stretch = distance - sphereRadius;
        Vec3f springforce = -stiffness * stretch * direction;

        force[i] += springforce;
      }
    }

    // Drag force (slows motion)
    for (int i = 0; i < velocity.size(); i++) {
      force[i] += -velocity[i] * dragforce;
    }

    // Integrate motion (Euler)
    for (int i = 0; i < velocity.size(); i++) {
      velocity[i] += force[i] / mass[i] * timeStep;
      position[i] += velocity[i] * timeStep;
    }

    // Clear forces for next frame
    for (auto &f : force) {
      f.set(0);
    }
  }

  void onDraw(Graphics &g) override {
    g.clear(0.3);

    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);

    // Bind font texture (needed for text rendering)
    g.texture();
    font.tex.bind();

    vector<Vec3f> &position = mesh.vertices();

    // Loop through each particle
    for (int i = 0; i < textMeshes.size(); i++) {

      g.pushMatrix();

      // Move to particle position
      g.translate(position[i]);

      // Scale text size
      g.scale(pointSize / 5.0);

      // Draw the text mesh at that position
      g.draw(textMeshes[i]);

      g.popMatrix();
    }

    font.tex.unbind();
  }
};

int main() {
  AlloApp app;
  app.start();
}